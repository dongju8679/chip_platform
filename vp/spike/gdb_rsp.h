// gdb_rsp.h - a GDB Remote Serial Protocol (RSP) server (for the spike backend)
//
// Why this is implemented in-house
//   Commercial SDKs (SiFive Freedom Studio etc.) use the path [spike|RTL] -> OpenOCD -> GDB.
//   We already own execution one instruction at a time via proc->step(1) in
//   verif_spike.h, so it is far thinner to skip the JTAG/DTM/OpenOCD layers
//   entirely and host an RSP server that GDB attaches to directly, inside our own process.
//     GDB  --(TCP:3333, RSP)-->  this server  -->  processor_t / simif_t
//   (The JTAG path is handled separately in vp/rtl stage 2 - see chip_docs/verif/docs/debugging.md)
//
// Directories/files used:
//   - riscv-isa-sim (libriscv) headers : processor.h (registers), simif.h (memory)
//   - vp/spike/verif_spike.h           : the step/done callback provider (injected by the driver)
//
// Called from:
//   - vp/spike/verif_spike_gdb.cc  (the main of the GDB-wait mode driver)
//
// Supported packets:
//   ?  qSupported  qXfer:features:read  qC/qAttached/qfThreadInfo  H  T
//   g/G  p/P  m/M  c/s/C/S  vCont?/vCont  Z0/z0 Z1/z1  k/D  QStartNoAckMode
#ifndef GDB_RSP_H
#define GDB_RSP_H

#if defined(SPIKE_NATIVE)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <riscv/processor.h>
#include <riscv/simif.h>

// -- The list of CSRs to expose --
//   Advertised through GDB's org.gnu.gdb.riscv.csr feature. The names must be
//   the standard CSR names GDB knows (per riscv-opc.h). This is an M-mode-only core, so M CSRs only.
struct gdb_csr_def_t { const char* name; int num; };
static const gdb_csr_def_t GDB_CSRS[] = {
  {"mstatus",  0x300}, {"misa",     0x301}, {"mie",      0x304},
  {"mtvec",    0x305}, {"mscratch", 0x340}, {"mepc",     0x341},
  {"mcause",   0x342}, {"mtval",    0x343}, {"mip",      0x344},
  {"mcycle",   0xB00}, {"minstret", 0xB02}, {"mhartid",  0xF14},
};
static const int GDB_NCSR = (int)(sizeof(GDB_CSRS) / sizeof(GDB_CSRS[0]));

// GPR ABI names (the names GDB expects in the org.gnu.gdb.riscv.cpu feature)
static const char* GDB_XREG_NAMES[32] = {
  "zero","ra","sp","gp","tp","t0","t1","t2",
  "fp","s1","a0","a1","a2","a3","a4","a5",
  "a6","a7","s2","s3","s4","s5","s6","s7",
  "s8","s9","s10","s11","t3","t4","t5","t6"
};

class gdb_rsp_server_t {
public:
  // step_fn : execute one instruction (the driver passes verif_spike_t::tick(1),
  //           so the timing model and mcycle mirroring stay intact while debugging)
  // done_fn : firmware termination decision (tohost != 0)
  // exit_fn : exit code (tohost >> 1)
  gdb_rsp_server_t(processor_t* proc, simif_t* simif,
                   std::function<void()> step_fn,
                   std::function<bool()> done_fn,
                   std::function<int()>  exit_fn)
    : proc(proc), simif(simif), step_fn(step_fn), done_fn(done_fn),
      exit_fn(exit_fn) {
    xlen = proc ? proc->get_xlen() : 32;
    reg_bytes = xlen / 8;
    build_target_xml();
  }

  ~gdb_rsp_server_t() {
    if (conn_fd >= 0) close(conn_fd);
    if (listen_fd >= 0) close(listen_fd);
  }

  // -- TCP listen + wait for GDB to connect --
  bool wait_for_gdb(int port) {
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("[gdb] socket"); return false; }
    int one = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // loopback only
    addr.sin_port = htons((uint16_t)port);
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
      fprintf(stderr, "[gdb] bind :%d failed: %s\n", port, strerror(errno));
      return false;
    }
    if (listen(listen_fd, 1) < 0) { perror("[gdb] listen"); return false; }

    fprintf(stderr,
      "[gdb] listening on 127.0.0.1:%d - from another terminal:\n"
      "[gdb]   riscv64-unknown-elf-gdb <elf> -ex 'target remote :%d'\n",
      port, port);
    conn_fd = accept(listen_fd, nullptr, nullptr);
    if (conn_fd < 0) { perror("[gdb] accept"); return false; }
    setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    fprintf(stderr, "[gdb] GDB connected (halted @ pc=0x%08llx)\n",
            (unsigned long long)get_pc());
    return true;
  }

  // -- Main loop: receive packet -> handle -> respond --
  void serve() {
    std::string pkt;
    while (conn_fd >= 0) {
      int r = recv_packet(pkt);
      if (r < 0) break;              // connection closed
      if (r == 0) continue;          // interrupt (0x03) etc. - ignored while halted
      if (!handle_packet(pkt)) break;
    }
    fprintf(stderr, "[gdb] session ended\n");
  }

private:
  // ------------------------------------------------------------
  // Register access
  // ------------------------------------------------------------
  // On RV32, spike keeps state.pc sign-extended to xlen.
  //   0x80000000 (DRAM) has bit31 set, so internally it becomes 0xFFFFFFFF80000000.
  //   On trap entry, however, the mtvec CSR value (zero-extended 32-bit) goes in as is.
  //   -> The two representations mix, so comparisons and hand-offs must always truncate to xlen.
  //     (Removing this masking produces the deeply confusing symptom that
  //      "breakpoints only trigger at the trap vector" - see chip_docs/verif/docs/debugging.md)
  uint64_t trunc_xlen(uint64_t v) const {
    return (xlen >= 64) ? v : (v & ((1ull << xlen) - 1));
  }
  uint64_t sext_xlen(uint64_t v) const {
    return (xlen >= 64) ? v : (uint64_t)(int64_t)(int32_t)(uint32_t)v;
  }

  uint64_t get_pc() const {
    return proc ? trunc_xlen(proc->get_state()->pc) : 0;
  }

  // regnum: 0..31 = x0..x31, 32 = pc, 33.. = GDB_CSRS[]
  uint64_t read_reg(int n) {
    if (!proc) return 0;
    state_t* st = proc->get_state();
    if (n >= 0 && n < 32) return trunc_xlen(st->XPR[n]);
    if (n == 32)          return trunc_xlen(st->pc);
    int c = n - 33;
    if (c >= 0 && c < GDB_NCSR) {
      try { return proc->get_csr(GDB_CSRS[c].num); }
      catch (...) { return 0; }      // unimplemented CSR -> 0 (GDB just displays it)
    }
    return 0;
  }

  void write_reg(int n, uint64_t v) {
    if (!proc) return;
    state_t* st = proc->get_state();
    // Sign-extend on the way in to match spike's internal convention (reads use trunc_xlen)
    if (n >= 0 && n < 32) { st->XPR.write(n, (reg_t)sext_xlen(v)); return; }
    if (n == 32)          { st->pc = (reg_t)sext_xlen(v); return; }
    int c = n - 33;
    if (c >= 0 && c < GDB_NCSR) {
      try { proc->put_csr(GDB_CSRS[c].num, (reg_t)v); } catch (...) {}
    }
  }

  int nregs() const { return 33 + GDB_NCSR; }

  // ------------------------------------------------------------
  // Memory access - a trap-free backdoor
  //   proc->get_mmu()->load<> throws a trap on a failed access and pollutes CSRs
  //   (mtval etc.). A debugger read must not alter program state, so we use
  //   simif_t's addr_to_mem (a host pointer) / mmio_load directly.
  // ------------------------------------------------------------
  bool mem_read(uint64_t addr, size_t len, uint8_t* out) {
    for (size_t i = 0; i < len; i++) {
      char* hp = simif->addr_to_mem((reg_t)(addr + i));
      if (hp) { out[i] = *(uint8_t*)hp; continue; }
      uint8_t b = 0;
      if (simif->mmio_load((reg_t)(addr + i), 1, &b)) { out[i] = b; continue; }
      return false;                  // unmapped -> return an error to GDB
    }
    return true;
  }

  bool mem_write(uint64_t addr, size_t len, const uint8_t* in) {
    for (size_t i = 0; i < len; i++) {
      char* hp = simif->addr_to_mem((reg_t)(addr + i));
      if (hp) { *(uint8_t*)hp = in[i]; continue; }
      if (simif->mmio_store((reg_t)(addr + i), 1, &in[i])) continue;
      return false;
    }
    return true;
  }

  // ------------------------------------------------------------
  // Execution control
  // ------------------------------------------------------------
  // Breakpoints are kept as a set of addresses rather than by patching ebreak into memory.
  //   - they also work in ROM/flash regions
  //   - the firmware image stays unpolluted, so they coexist with CA timing measurement
  //   - we step one instruction at a time anyway, so the cost is effectively the same
  std::set<uint64_t> breakpoints;

  // continue: run until a breakpoint, termination, or Ctrl-C
  void do_continue() {
    const int POLL_EVERY = 4096;
    long n = 0;
    while (true) {
      step_fn();
      if (done_fn()) { send_exited(); return; }
      if (breakpoints.count(get_pc())) { send_stop_bp(); return; }
      if (++n % POLL_EVERY == 0 && check_interrupt()) { send_stop(2); return; }
    }
  }

  void do_step() {
    step_fn();
    if (done_fn()) { send_exited(); return; }
    send_stop(5);
  }

  // Non-blocking check for a Ctrl-C (0x03) sent by GDB
  bool check_interrupt() {
    char c;
    int r = recv(conn_fd, &c, 1, MSG_DONTWAIT);
    return (r == 1 && c == 0x03);
  }

  void send_stop(int sig) {
    char buf[32];
    snprintf(buf, sizeof(buf), "T%02xthread:1;", sig);
    send_packet(buf);
  }
  void send_stop_bp() { send_packet("T05thread:1;swbreak:;"); }
  void send_exited() {
    exited = true;
    char buf[16];
    int code = exit_fn();
    snprintf(buf, sizeof(buf), "W%02x", code & 0xFF);
    fprintf(stderr, "[gdb] firmware terminated (tohost, exit_code=%d)\n", code);
    send_packet(buf);
  }

  // ------------------------------------------------------------
  // target.xml (qXfer:features:read) - pins down the g packet layout
  //   Without it GDB assumes its built-in default tdesc (which includes an FPU)
  //   and the register count mismatches. This is rv32imac (no FPU), so we advertise it ourselves.
  // ------------------------------------------------------------
  void build_target_xml() {
    char buf[256];
    target_xml  = "<?xml version=\"1.0\"?>\n";
    target_xml += "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n<target>\n";
    snprintf(buf, sizeof(buf), "  <architecture>riscv:rv%u</architecture>\n", xlen);
    target_xml += buf;
    target_xml += "  <feature name=\"org.gnu.gdb.riscv.cpu\">\n";
    for (int i = 0; i < 32; i++) {
      snprintf(buf, sizeof(buf),
               "    <reg name=\"%s\" bitsize=\"%u\" type=\"%s\" regnum=\"%d\"/>\n",
               GDB_XREG_NAMES[i], xlen,
               (i == 2 || i == 8) ? "data_ptr" : "int", i);
      target_xml += buf;
    }
    snprintf(buf, sizeof(buf),
             "    <reg name=\"pc\" bitsize=\"%u\" type=\"code_ptr\" regnum=\"32\"/>\n", xlen);
    target_xml += buf;
    target_xml += "  </feature>\n";
    target_xml += "  <feature name=\"org.gnu.gdb.riscv.csr\">\n";
    for (int i = 0; i < GDB_NCSR; i++) {
      snprintf(buf, sizeof(buf),
               "    <reg name=\"%s\" bitsize=\"%u\" type=\"int\" regnum=\"%d\"/>\n",
               GDB_CSRS[i].name, xlen, 33 + i);
      target_xml += buf;
    }
    target_xml += "  </feature>\n</target>\n";
  }

  // ------------------------------------------------------------
  // Packet handling
  // ------------------------------------------------------------
  bool handle_packet(const std::string& p) {
    if (p.empty()) { send_packet(""); return true; }

    switch (p[0]) {
    case '?':                                  // why are we halted
      send_stop(5); return true;

    case 'q': case 'Q':
      return handle_query(p);

    case 'H':                                  // thread selection - single hart
      send_packet("OK"); return true;

    case 'T':                                  // is the thread alive
      send_packet("OK"); return true;

    case 'g': {                                // read all registers
      std::string out;
      for (int i = 0; i < nregs(); i++) out += hex_le(read_reg(i), reg_bytes);
      send_packet(out); return true;
    }
    case 'G': {                                // write all registers
      const char* s = p.c_str() + 1;
      for (int i = 0; i < nregs() && strlen(s) >= (size_t)reg_bytes * 2; i++) {
        write_reg(i, unhex_le(s, reg_bytes));
        s += reg_bytes * 2;
      }
      send_packet("OK"); return true;
    }
    case 'p': {                                // read a single register
      int n = (int)strtol(p.c_str() + 1, nullptr, 16);
      send_packet(hex_le(read_reg(n), reg_bytes)); return true;
    }
    case 'P': {                                // write a single register  P n=vvvv
      size_t eq = p.find('=');
      if (eq == std::string::npos) { send_packet("E01"); return true; }
      int n = (int)strtol(p.substr(1, eq - 1).c_str(), nullptr, 16);
      write_reg(n, unhex_le(p.c_str() + eq + 1, reg_bytes));
      send_packet("OK"); return true;
    }

    case 'm': {                                // read memory  m addr,len
      uint64_t addr = 0; size_t len = 0;
      if (!parse_addr_len(p.c_str() + 1, addr, len)) { send_packet("E01"); return true; }
      if (len > 4096) len = 4096;
      std::vector<uint8_t> b(len);
      if (!mem_read(addr, len, b.data())) { send_packet("E14"); return true; }
      std::string out;
      for (size_t i = 0; i < len; i++) out += byte_hex(b[i]);
      send_packet(out); return true;
    }
    case 'M': {                                // write memory  M addr,len:data
      uint64_t addr = 0; size_t len = 0;
      size_t colon = p.find(':');
      if (colon == std::string::npos ||
          !parse_addr_len(p.c_str() + 1, addr, len)) { send_packet("E01"); return true; }
      std::vector<uint8_t> b(len);
      const char* s = p.c_str() + colon + 1;
      for (size_t i = 0; i < len; i++) b[i] = (uint8_t)unhex_le(s + i * 2, 1);
      send_packet(mem_write(addr, len, b.data()) ? "OK" : "E14"); return true;
    }

    case 'c': case 'C':                        // continue (signal ignored)
      if (exited) { send_exited(); return true; }
      do_continue(); return true;
    case 's': case 'S':                        // single step
      if (exited) { send_exited(); return true; }
      do_step(); return true;

    case 'v':
      return handle_v(p);

    case 'Z': case 'z': {                      // breakpoint set/clear
      // Z<type>,<addr>,<kind>   type 0=sw, 1=hw (handled identically), 2~4=watch (unsupported)
      if (p.size() < 4 || (p[1] != '0' && p[1] != '1')) { send_packet(""); return true; }
      uint64_t addr = trunc_xlen(strtoull(p.c_str() + 3, nullptr, 16));
      if (p[0] == 'Z') breakpoints.insert(addr);
      else             breakpoints.erase(addr);
      send_packet("OK"); return true;
    }

    case 'D':                                  // detach
      send_packet("OK"); return false;
    case 'k':                                  // kill
      return false;

    default:
      send_packet("");                         // unsupported -> empty response
      return true;
    }
  }

  bool handle_query(const std::string& p) {
    if (p.rfind("qSupported", 0) == 0) {
      send_packet("PacketSize=4000;qXfer:features:read+;swbreak+;hwbreak+;"
                  "vContSupported+;QStartNoAckMode+");
      return true;
    }
    if (p == "QStartNoAckMode") { send_packet("OK"); no_ack = true; return true; }
    if (p.rfind("qXfer:features:read:", 0) == 0) {
      // qXfer:features:read:<annex>:<offset>,<length>
      size_t c1 = p.rfind(':');
      size_t comma = p.find(',', c1);
      unsigned long off = strtoul(p.c_str() + c1 + 1, nullptr, 16);
      unsigned long len = strtoul(p.c_str() + comma + 1, nullptr, 16);
      if (off >= target_xml.size()) { send_packet("l"); return true; }
      std::string chunk = target_xml.substr(off, len);
      send_packet((off + chunk.size() < target_xml.size() ? "m" : "l") + chunk);
      return true;
    }
    if (p == "qC")            { send_packet("QC1");  return true; }
    if (p.rfind("qAttached", 0) == 0) { send_packet("1"); return true; }
    if (p == "qfThreadInfo")  { send_packet("m1");  return true; }
    if (p == "qsThreadInfo")  { send_packet("l");   return true; }
    if (p.rfind("qSymbol", 0) == 0) { send_packet("OK"); return true; }
    if (p.rfind("qRcmd,", 0) == 0)  return handle_monitor(p.substr(6));
    send_packet("");
    return true;
  }

  // monitor <cmd>  - the window for inspecting backend state from the GDB console
  bool handle_monitor(const std::string& hexcmd) {
    std::string cmd;
    for (size_t i = 0; i + 1 < hexcmd.size(); i += 2)
      cmd += (char)unhex_le(hexcmd.c_str() + i, 1);
    char out[256];
    if (cmd.rfind("cycles", 0) == 0) {
      snprintf(out, sizeof(out), "timing-model cycles = %llu\n",
               (unsigned long long)read_reg(33 + 9));   // mcycle
    } else if (cmd.rfind("reset", 0) == 0) {
      proc->get_state()->pc = entry_pc;
      snprintf(out, sizeof(out), "pc <- 0x%08llx (entry)\n",
               (unsigned long long)entry_pc);
    } else {
      snprintf(out, sizeof(out), "usage: monitor [cycles|reset]\n");
    }
    std::string hex;
    for (const char* q = out; *q; q++) hex += byte_hex((uint8_t)*q);
    send_packet(hex);
    return true;
  }

  bool handle_v(const std::string& p) {
    if (p.rfind("vCont?", 0) == 0) { send_packet("vCont;c;C;s;S"); return true; }
    if (p.rfind("vCont", 0) == 0) {
      // vCont;<action>[:tid][;...]   - only the first action is considered (single hart)
      size_t i = 5;
      if (i < p.size() && p[i] == ';') i++;
      char act = (i < p.size()) ? p[i] : 'c';
      if (exited) { send_exited(); return true; }
      if (act == 's' || act == 'S') do_step();
      else                          do_continue();
      return true;
    }
    if (p.rfind("vKill", 0) == 0) { send_packet("OK"); return false; }
    if (p.rfind("vMustReplyEmpty", 0) == 0) { send_packet(""); return true; }
    send_packet("");
    return true;
  }

  // ------------------------------------------------------------
  // Low-level RSP I/O
  // ------------------------------------------------------------
  // Returns: 1=packet received, 0=control character only, -1=connection closed
  int recv_packet(std::string& out) {
    out.clear();
    char c;
    // Skip up to '$' (consuming +/-/0x03)
    while (true) {
      int r = ::recv(conn_fd, &c, 1, 0);
      if (r <= 0) return -1;
      if (c == '$') break;
      if (c == 0x03) return 0;
    }
    uint8_t sum = 0;
    while (true) {
      int r = ::recv(conn_fd, &c, 1, 0);
      if (r <= 0) return -1;
      if (c == '#') break;
      out += c;
      sum = (uint8_t)(sum + (uint8_t)c);
    }
    char ck[2];
    if (::recv(conn_fd, &ck[0], 1, 0) <= 0) return -1;
    if (::recv(conn_fd, &ck[1], 1, 0) <= 0) return -1;
    uint8_t want = (uint8_t)((hexval(ck[0]) << 4) | hexval(ck[1]));
    if (!no_ack) {
      const char ack = (sum == want) ? '+' : '-';
      if (::send(conn_fd, &ack, 1, 0) != 1) return -1;
    }
    if (sum != want) { out.clear(); return 0; }
    if (verbose) fprintf(stderr, "[gdb] <- %s\n", out.c_str());
    return 1;
  }

  void send_packet(const std::string& payload) {
    std::string msg = "$" + payload + "#";
    uint8_t sum = 0;
    for (char c : payload) sum = (uint8_t)(sum + (uint8_t)c);
    msg += byte_hex(sum);
    if (verbose) fprintf(stderr, "[gdb] -> %s\n", payload.c_str());
    size_t sent = 0;
    while (sent < msg.size()) {
      ssize_t n = ::send(conn_fd, msg.data() + sent, msg.size() - sent, 0);
      if (n <= 0) { close(conn_fd); conn_fd = -1; return; }
      sent += (size_t)n;
    }
    if (!no_ack) {                            // consume GDB's ack
      char a;
      while (::recv(conn_fd, &a, 1, 0) == 1) {
        if (a == '+') break;
        if (a == '-') { ::send(conn_fd, msg.data(), msg.size(), 0); }
      }
    }
  }

  // -- hex helpers --
  static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  }
  static std::string byte_hex(uint8_t b) {
    static const char* H = "0123456789abcdef";
    std::string s; s += H[b >> 4]; s += H[b & 0xF]; return s;
  }
  // RSP exchanges registers and memory as hex in target-endian (little) byte order
  static std::string hex_le(uint64_t v, int nbytes) {
    std::string s;
    for (int i = 0; i < nbytes; i++) s += byte_hex((uint8_t)((v >> (8 * i)) & 0xFF));
    return s;
  }
  static uint64_t unhex_le(const char* s, int nbytes) {
    uint64_t v = 0;
    for (int i = 0; i < nbytes; i++) {
      uint8_t b = (uint8_t)((hexval(s[i * 2]) << 4) | hexval(s[i * 2 + 1]));
      v |= (uint64_t)b << (8 * i);
    }
    return v;
  }
  static bool parse_addr_len(const char* s, uint64_t& addr, size_t& len) {
    char* end = nullptr;
    addr = strtoull(s, &end, 16);
    if (!end || *end != ',') return false;
    len = (size_t)strtoull(end + 1, nullptr, 16);
    return true;
  }

  processor_t* proc;
  simif_t* simif;
  std::function<void()> step_fn;
  std::function<bool()> done_fn;
  std::function<int()>  exit_fn;

  unsigned xlen = 32;
  int reg_bytes = 4;
  std::string target_xml;
  int listen_fd = -1, conn_fd = -1;
  bool no_ack = false, exited = false;

public:
  bool verbose = false;      // packet logging via +gdbverbose
  uint64_t entry_pc = 0;     // for monitor reset (set by the driver)
};

#endif // SPIKE_NATIVE
#endif // GDB_RSP_H
