// verif_spike_gdb.cc - the spike backend GDB debug driver
//
// A sibling of verif_spike_run.cc. It uses the same adapter (verif_spike_t) and
//   the same ELF, but the loop is owned by someone else:
//     verif_spike_run : while(!done) { tick; idle; }     - verification drives it
//     verif_spike_gdb : tick(1) only when GDB sends c/s  - the debugger drives it
//   The existing driver and interface are untouched (this is a separate executable).
//
// Directories/files used:
//   - vp/spike/verif_spike.h  (the adapter - tick/done/exit_code/get_proc/get_simif)
//   - vp/spike/gdb_rsp.h      (the RSP server)
//   - build/<TEST>.elf        (the firmware under debug; build with -g for source level)
//
// Build:  RISCV=... vp/spike/build.sh   ->  vp/spike/verif_spike_gdb
// Usage:
//   Terminal A:  ./vp/spike/verif_spike_gdb build/UART_printf.elf +gdb=3333
//   Terminal B:  riscv64-unknown-elf-gdb build/UART_printf.elf \
//                -ex 'target remote :3333' -ex 'break main' -ex 'continue'

#include <cstdio>
#include <cstring>
#include <string>

#include "verif_spike.h"
#include "gdb_rsp.h"

int main(int argc, char** argv) {
  // -- 1) Argument parsing --  <elf> [+gdb=<port>] [+isa=..] [+tohost=0x..] [+gdbverbose]
  std::string elf_path, isa = "rv32imac_zicsr_zifencei";
  int port = 3333;
  uint64_t tohost_addr = 0;
  bool verbose = false;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if      (a.rfind("+gdb=", 0) == 0)    port = atoi(a.substr(5).c_str());
    else if (a == "+gdbverbose")          verbose = true;
    else if (a.rfind("+isa=", 0) == 0)    isa = a.substr(5);
    else if (a.rfind("+tohost=", 0) == 0) tohost_addr = strtoull(a.substr(8).c_str(), nullptr, 0);
    else if (a[0] != '+')                 elf_path = a;
  }
  if (elf_path.empty()) {
    fprintf(stderr,
      "usage: verif_spike_gdb <elf> [+gdb=<port>] [+isa=<isa>] [+tohost=<hex>] [+gdbverbose]\n");
    return 2;
  }

#if !defined(SPIKE_NATIVE)
  fprintf(stderr, "[verif_spike_gdb] this is not a SPIKE_NATIVE build "
                  "(set RISCV=<libriscv prefix> and re-run build.sh)\n");
  return 2;
#else
  // -- 2) Adapter construction + ELF loading --
  verif_spike_t v(isa);
  if (!v.load_elf(elf_path)) {
    fprintf(stderr, "[verif_spike_gdb] load_elf failed: %s\n", elf_path.c_str());
    return 2;
  }
  if (tohost_addr) v.set_tohost(tohost_addr);

  processor_t* proc = v.get_proc();
  simif_t* simif = v.get_simif();
  if (!proc || !simif) {
    fprintf(stderr, "[verif_spike_gdb] sim construction failed\n");
    return 2;
  }

  // -- 3) RSP server --
  //   step uses v.tick(1) directly -> the timing model keeps accumulating and
  //   mcycle mirroring is preserved even while debugging (mcycle in `info registers` = the model cycles).
  gdb_rsp_server_t gdb(proc, simif,
                       [&v]() { v.tick(1); },
                       [&v]() { return v.done(); },
                       [&v]() { return v.exit_code(); });
  gdb.verbose = verbose;
  // On RV32 spike sign-extends the pc, so it is truncated to 32 bits for storage (see the gdb_rsp.h comment)
  gdb.entry_pc = (uint64_t)proc->get_state()->pc & 0xFFFFFFFFull;

  fprintf(stderr, "[verif_spike_gdb] elf=%s  entry=0x%08llx  halted, waiting for GDB\n",
          elf_path.c_str(), (unsigned long long)gdb.entry_pc);

  if (!gdb.wait_for_gdb(port)) return 2;
  gdb.serve();
  return 0;
#endif
}
