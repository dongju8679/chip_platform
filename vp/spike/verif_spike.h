// verif_spike.h - the spike backend adapter (the spike implementation of verif_primitives_t)
//
// Implements the framework seam (VERIF/include/verif_primitives.h) on top of
// spike (riscv-isa-sim, libriscv). A sibling of chipyard's verif_tsi_t - same interface, different engine.
//
//   master_write/read -> mmu->store_uint32 / load_uint32
//   trigger_irq       -> state.mip |= (1<<id)   unlike chipyard, set directly (easy)
//   tick              -> proc->step(n)          we control the advance of time ourselves
//
// Directories/files used:
//   - VERIF/include/verif_primitives.h   (the interface being inherited + the host_*bp helpers)
//   - riscv-isa-sim (libriscv) headers   (external install: riscv/sim.h, processor.h, mmu.h)
//
// Called from: main() in backend/spike/verif_spike_run.cc constructs and drives this class.
#ifndef VERIF_SPIKE_H
#define VERIF_SPIKE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

// -- DUT memory map: spike uses the ELF's addresses as is (virtual platform memory) --
//   chipyard has DRAM @0x80000000 so it was overridden there, but spike loads at
//   the ELF's link addresses, so the official SDK values (0x50000/0x51000) can be used unchanged.
//   Overridable with -D if needed (thanks to the #ifndef guards in verif_primitives.h).
#ifndef VERIF_TEST_ADDR
#define VERIF_TEST_ADDR   0x50000u
#endif
#ifndef VERIF_BREAK_ADDR
#define VERIF_BREAK_ADDR  0x51000u
#endif

#include "verif_primitives.h"   // seam (-I VERIF/include). The BP_* numbers are shared.
#include "elf_tohost.h"         // standalone ELF tohost parser (independent of the spike version)
#include "timing_model.h"       // the CA timing model (Phase 2: instruction relationships + address-aware latency)

// -- CA_measure integration addresses (the DRAM mailbox of the chipyard baremetal-track ELF) --
//   CA_measure.elf is linked for chipyard (0x80000000 DRAM), so the mailbox is in DRAM too.
//   Identical to the values in verif/backend/chipyard/verif_host.h and Platform/Common/Inc/verif_ca.h.
#define VERIF_CA_BP_ADDR       0x80010000u   // BREAK_POINT (util.h)
#define VERIF_CA_CYCLE_LO_ADDR 0x80010040u   // DUT mcycle mirror
#define VERIF_CA_CYCLE_HI_ADDR 0x80010044u
#define VERIF_CA_INSTR_LO_ADDR 0x80010048u
#define VERIF_CA_INSTR_HI_ADDR 0x8001004Cu
#define VERIF_CA_TABLE_ADDR    0x80010100u   // CA result table
#define VERIF_CA_MAGIC         0xCA5EC0DEu
#define VERIF_CA_ENTRY_SZ      32u

// -- riscv-isa-sim (libriscv) headers --
//   Requires an external install: $RISCV/include/riscv/*.h (configure --prefix=$RISCV; make install)
//   build.sh links with -DSPIKE_NATIVE -I$RISCV/include -lriscv.
//   Guarded so it still compiles without these headers in offline (porting/CI) builds.
#if defined(SPIKE_NATIVE)
#include <riscv/sim.h>
#include <riscv/processor.h>
#include <riscv/mmu.h>
#include "spike_factory.h"      // sim_t construction (isolates version differences)
#endif
#include <memory>
#include <vector>

// verif_spike_t = the spike adapter. Implements verif_primitives_t (the interface).
//   Unlike chipyard's verif_tsi_t, it does not inherit an engine such as testchip_tsi_t.
//   Instead it owns a sim_t (the spike core) as a member and drives it directly.
class verif_spike_t : public verif_primitives_t {
public:
  verif_spike_t(const std::string& isa = "rv32imac_zicsr_zifencei",
                const std::string& priv = "m")
    : isa_str(isa), priv_str(priv), proc(nullptr),
      tohost_addr(0), ran(false) {
    // sim_t/processor_t construction happens in load_elf together with the ELF info (a cfg must be built).
    // Only member initialization here. (The real sim_t ctor takes cfg_t/mem/plugin
    //  arguments differently per version, so it is built in load_elf.)
    // VP_TIMING_PHASE=1 -> revert to the Phase 1 model (for A/B comparison)
    if (const char* ph = getenv("VP_TIMING_PHASE"))
      if (ph[0] == '1') timing.phase = 1;
    fprintf(stderr, "[verif_spike] ACTIVE (spike backend, isa=%s, timing phase %d)\n",
            isa.c_str(), timing.phase);
  }

  // -- ELF loading + sim construction --
  //   Directory used: Implementation/build/<TEST>.elf (the firmware built by Makefile.VERIF)
  //   Obtains the tohost symbol address (polled by done()/exit_code()).
  bool load_elf(const std::string& elf_path) {
    elf = elf_path;

    // (1) tohost symbol address - obtained with the standalone ELF parser (version independent, always works).
    tohost_addr = elf_find_tohost(elf_path);
    if (!tohost_addr) {
      fprintf(stderr, "[verif_spike] WARN: tohost not found in %s\n", elf_path.c_str());
    }

    // (2) sim_t construction - binds to the installed riscv-isa-sim (in SPIKE_NATIVE builds).
    //     ctor signatures differ per version, so the binding is separated with #if.
#if defined(SPIKE_NATIVE)
    //   Adjust the cfg_t / mem / plugin arguments to the installed version (the sequence is the same):
    //   std::vector<std::pair<reg_t,abstract_mem_t*>> mems = make_mems(cfg.mem_layout());
    //   sim = std::make_unique<sim_t>(&cfg, /*halted=*/false, mems, plugin_devices,
    //                                 std::vector<std::string>{elf_path}, ...);
    //   proc = sim->get_core(0);
    sim  = make_spike_sim(isa_str, elf_path);   // a thin factory (vp/spike/spike_factory.h)
    proc = sim ? sim->get_core(0) : nullptr;
    if (!tohost_addr && sim) tohost_addr = sim->get_tohost_addr();
#else
    // Offline (porting/CI) build: no sim_t is constructed. Only the tohost address is known.
    //   Actual execution requires a SPIKE_NATIVE build linked against libriscv.
    fprintf(stderr, "[verif_spike] load_elf: %s (tohost=0x%llx) "
                    "(execution requires a SPIKE_NATIVE build)\n",
            elf_path.c_str(), (unsigned long long)tohost_addr);
#endif
    return true;
  }

  void set_test(const std::string& t) { test = t; }

  void set_tohost(uint64_t addr) { tohost_addr = addr; }

  // ------------------------------------------------------------
  // seam implementation (verif_primitives_t) - the 4 base primitives
  // ------------------------------------------------------------

  // host -> DUT memory write.  mmu->store<uint32_t>
  bool master_write(uint32_t addr, uint32_t data) override {
    if (!proc) return false;
#if defined(SPIKE_NATIVE)
    try { proc->get_mmu()->store<uint32_t>((reg_t)addr, data); }
    catch (...) { return false; }
#endif
    return true;
  }

  // host <- DUT memory read.  mmu->load<uint32_t>
  bool master_read(uint32_t addr, uint32_t& data) override {
    if (!proc) return false;
#if defined(SPIKE_NATIVE)
    try { data = proc->get_mmu()->load<uint32_t>((reg_t)addr); }
    catch (...) { return false; }
#else
    (void)addr; data = 0;
#endif
    return true;
  }

  // host -> DUT interrupt injection.  sets state.mip directly
  //   chipyard would need a HarnessBinder/DPI and is unimplemented there. On spike it is this one line.
  //   (In the installed version mip is a csr object -> set via backdoor_write_with_mask)
  void trigger_irq(uint32_t irq_id) override {
    if (!proc) return;
#if defined(SPIKE_NATIVE)
    reg_t m = (reg_t)1u << irq_id;
    proc->get_state()->mip->backdoor_write_with_mask(m, m);
#else
    (void)irq_id;
#endif
  }

  // Advance simulation time + accumulate CA timing.
  //   Function first (step), timing second (account) - one instruction at a time.
  //   On chipyard this is a no-op (the idle read advances things), but on spike we
  //   step it ourselves and accumulate each instruction's cycles through timing_model (= pure C cycle prediction).
  //
  //   The Phase 2 supply flow (things only possible because it is execution driven):
  //     (1) Just before the step: fetch and decode the instruction at PC.
  //         For a load/store/amo, read the "real value" of the base register from
  //         get_state()->XPR and compute the access address (addr = XPR[base] + offset).
  //         This is the execution-driven advantage: reading live registers without a commit log.
  //     (2) Execute functionally with proc->step(1).
  //     (3) Compare minstret across the step - if unchanged, no instruction
  //         committed and a trap/interrupt was entered -> account_trap(). If it committed, account(ctx)
  //         (ctx.next_pc = the PC after the step -> the input for branch-taken detection).
  //     (4) Mirror the model's accumulated cycles into spike's mcycle CSR.
  //         -> The firmware's csrr mcycle then reads "the timing model's cycles",
  //           so self-timing firmware such as CA_measure can be reused unmodified.
  //           (minstret keeps spike's original value = the exact commit count -> CPI stays consistent)
  void tick(int n) override {
    if (!proc) return;
#if defined(SPIKE_NATIVE)
    state_t* st = proc->get_state();
    for (int i = 0; i < n; i++) {
      // (1) Collect the context just before execution
      insn_ctx_t ctx;
      ctx.pc = st->pc;
      try { ctx.insn = (uint32_t)proc->get_mmu()->load_insn(ctx.pc).insn.bits(); }
      catch (...) { ctx.insn = 0x13; /* cannot fetch -> treat as nop */ }
      // load/store/amo: compute the access address from the real base register value (the ED advantage)
      rvclass::mem_ref_t mr = rvclass::mem_ref(ctx.insn);
      if (mr.valid) {
        ctx.has_mem_addr = true;
        ctx.mem_addr = (uint64_t)(uint32_t)((uint32_t)st->XPR[mr.base] +
                                            (uint32_t)mr.offset);
      }
      uint64_t instret0 = st->minstret->read();
      // (2) Functional execution (spike)
      proc->step(1);
      // (3) Timing accumulation (timing_model)
      if (st->minstret->read() == instret0) {
        // No instruction committed = trap/interrupt entry (including the trigger_irq path in verif_ca_run)
        timing.account_trap();
      } else {
        ctx.next_pc = st->pc;
        timing.account(ctx);
      }
      // (4) mcycle <- mirror of the model cycles (the firmware's csrr mcycle reads the model value)
      st->mcycle->write((reg_t)timing.cycles());
    }
#else
    (void)n;
#endif
  }

  // -- CA extension -- cycle prediction with a pure C timing model (no verilator needed)
  //   get_cycle() = the accumulated timing_model cycles. supports_ca()=true.
  //   Place it side by side with chipyard (RTL) to compare trends -> assess confidence -> calibrate the model.
  //   Even without hardware, this value alone allows cycle prediction and analysis.
  uint64_t get_cycle() override {
    return timing.cycles();   // the accumulated predicted cycles (the sum of per-instruction-group costs)
  }
  bool supports_ca() override { return true; }   // CA is supported via the C model

  // host_set_bp / host_wait_bp / host_check_bp use the verif_primitives_t default implementations
  // (built on top of master_* + tick - shared and backend independent).

  // -- Handles for debugger (GDB RSP) access -- additive only (no effect on existing paths)
  //   vp/spike/verif_spike_gdb.cc passes these to gdb_rsp_server_t.
  //   Registers are viewed through processor_t, memory through simif_t (a trap-free backdoor).
#if defined(SPIKE_NATIVE)
  processor_t* get_proc() { return proc; }
  simif_t*     get_simif() { return sim.get(); }
#endif

  // ------------------------------------------------------------
  // Verification loop entry point (called by main in verif_spike_run.cc)
  // ------------------------------------------------------------

  // Called on every host turn. Dispatches run_vseq only on the first call (the same pattern as chipyard's idle).
  void idle() {
    if (ran) return;
    ran = true;
    run_vseq();
  }

  // Poll tohost -> termination decision.  mmu->load<uint64_t>(tohost_addr)
  bool done() {
    if (!proc || tohost_addr == 0) return false;
#if defined(SPIKE_NATIVE)
    try { return proc->get_mmu()->load<uint64_t>((reg_t)tohost_addr) != 0; }
    catch (...) { return false; }   // riscv-tests: t&1 == done
#else
    return false;
#endif
  }

  // PASS/FAIL code.  tohost >> 1  (the riscv-tests convention)
  int exit_code() {
    if (!proc || tohost_addr == 0) return -1;
#if defined(SPIKE_NATIVE)
    try { return (int)(proc->get_mmu()->load<uint64_t>((reg_t)tohost_addr) >> 1); }
    catch (...) { return -1; }
#else
    return 0;
#endif   // 0 means PASS, anything else FAIL(code)
  }

private:
  std::string isa_str, priv_str, elf, test;
#if defined(SPIKE_NATIVE)
  std::unique_ptr<sim_t> sim;     // the spike core container (created in load_elf)
  processor_t* proc = nullptr;    // hart 0 (sim->get_core(0))
#else
  void* proc = nullptr;           // offline: no type needed (sim_t is unused)
#endif
  timing_model_t timing;          // the CA cycle accumulation model
  uint64_t tohost_addr;
  bool ran;

  // -- Per-test host sequences (the same logic as run_vseq in chipyard's verif_host.h) --
  //   This code is backend independent - 100% shareable with chipyard.
  //   Directory used: the sequences in VERIF/<TARGET>/<F>/<SF>/Vseq/*.cc are their proper home.
  //   (Currently dispatched inline; when Vseq integration lands, those files will be called)
  void run_vseq() {
    // self-check: no host involvement (the firmware judges itself)
    if (test == "CLINT_rtc" || test.empty()) return;

    // co-sim example: DEMO_handshake (the host checks the value)
    if (test == "DEMO_handshake") {
      if (!host_wait_bp(BP_TEST_begin)) {
        fprintf(stderr, "[verif_spike] FAIL wait bp\n");
        return;
      }
      uint32_t got = 0; master_read(VERIF_TEST_ADDR, got);
      bool ok = (got == 0xABCDu);
      fprintf(stderr, "[verif_spike] DEMO_handshake %s (got 0x%X)\n",
              ok ? "PASS" : "FAIL", got);
      master_write(VERIF_BREAK_ADDR, BP_TEST_end);
      return;
    }

    // co-sim example: PLIC_enable (the host injects an interrupt) - a spike strength
    if (test == "PLIC_enable") {
      if (!host_wait_bp(BP_TEST_begin)) return;
      trigger_irq(/*irq_id=*/3);                    // * mip set
      if (!host_wait_bp(BP_TEST_end)) return;        // wait for the ISR to be handled
      uint32_t got = 0; master_read(VERIF_TEST_ADDR, got);
      fprintf(stderr, "[verif_spike] PLIC_enable %s (got 0x%X)\n",
              (got == 1u) ? "PASS" : "FAIL", got);
      return;
    }

    // -- CA_measure: per-block CPI (spike execution-driven + timing model) --
    //   The same table and output format as the CA_measure dispatch in verif_host.h (chipyard RTL).
    //   Difference: the mcycle the firmware reads here is not "spike's real clock"
    //   but the timing_model's predicted cycles, mirrored in by tick().
    //   -> Placing this table alongside the RTL measurements (build/logs/CA_measure.log)
    //     shows whether the Phase 2 model reproduces the stall trends (LDUSE>LDINDEP etc.).
    if (test == "CA_measure") {
      if (!ca_wait_bp(BP_TEST_end)) {
        fprintf(stderr, "[verif_spike] CA_measure FAIL (DUT never reached BP_TEST_end)\n");
        return;
      }
      uint64_t cyc_end = 0, ins_end = 0;
      { uint32_t lo=0, hi=0;
        master_read(VERIF_CA_CYCLE_LO_ADDR, lo); master_read(VERIF_CA_CYCLE_HI_ADDR, hi);
        cyc_end = ((uint64_t)hi << 32) | lo;
        master_read(VERIF_CA_INSTR_LO_ADDR, lo); master_read(VERIF_CA_INSTR_HI_ADDR, hi);
        ins_end = ((uint64_t)hi << 32) | lo; }

      uint32_t magic = 0, count = 0;
      master_read(VERIF_CA_TABLE_ADDR + 0, magic);
      master_read(VERIF_CA_TABLE_ADDR + 4, count);
      if (magic != VERIF_CA_MAGIC || count == 0 || count > 24) {
        fprintf(stderr, "[verif_spike] CA_measure FAIL (magic=0x%08x count=%u)\n",
                magic, count);
        return;
      }

      struct ca_row { char name[9]; uint32_t cyc, ins, iter, ncyc, nins; };
      std::vector<ca_row> rows;
      uint32_t ov_cyc = 0, ov_ins = 0;
      fprintf(stderr,
        "[verif_spike] ---- CA_measure : spike execution-driven + timing model (Phase 2) ----\n");
      fprintf(stderr, "[verif_spike] %-9s %9s %9s %9s %9s %8s %10s\n",
              "block", "cyc_raw", "ins_raw", "cyc_net", "ins_net", "CPI", "cyc/iter");
      for (uint32_t i = 0; i < count; i++) {
        uint32_t base = VERIF_CA_TABLE_ADDR + 16 + i * VERIF_CA_ENTRY_SZ;
        uint32_t w0=0, w1=0, cyc=0, ins=0, iter=0, dummy=0;
        master_read(base + 0,  w0);
        master_read(base + 4,  w1);
        master_read(base + 8,  cyc);
        master_read(base + 12, dummy);           // cyc_hi (0 in this range)
        master_read(base + 16, ins);
        master_read(base + 20, dummy);           // ins_hi
        master_read(base + 24, iter);
        ca_row r;
        for (int b = 0; b < 4; b++) r.name[b]     = (char)((w0 >> (8*b)) & 0xFF);
        for (int b = 0; b < 4; b++) r.name[4 + b] = (char)((w1 >> (8*b)) & 0xFF);
        r.name[8] = '\0';
        r.cyc = cyc; r.ins = ins; r.iter = iter;
        if (i == 0) { ov_cyc = cyc; ov_ins = ins; }   // the first entry = OVERHEAD
        r.ncyc = (cyc > ov_cyc) ? (cyc - ov_cyc) : 0;
        r.nins = (ins > ov_ins) ? (ins - ov_ins) : 0;
        rows.push_back(r);
        if (i == 0)
          fprintf(stderr, "[verif_spike] %-9s %9u %9u %9s %9s %8s %10s  (calibration)\n",
                  r.name, cyc, ins, "-", "-", "-", "-");
        else
          fprintf(stderr, "[verif_spike] %-9s %9u %9u %9u %9u %8.3f %10.2f\n",
                  r.name, cyc, ins, r.ncyc, r.nins,
                  r.nins ? (double)r.ncyc / (double)r.nins : 0.0,
                  r.iter ? (double)r.ncyc / (double)r.iter : 0.0);
      }
      fprintf(stderr, "[verif_spike] ------------------------------------------------------------\n");

      // controlled pairs: the cycle difference between two blocks with equal instruction counts = pure stall
      auto find_row = [&](const char* n) -> const ca_row* {
        for (size_t k = 0; k < rows.size(); k++)
          if (std::string(rows[k].name) == n) return &rows[k];
        return nullptr;
      };
      struct { const char* a; const char* b; uint32_t n; const char* what; } pairs[] = {
        {"LDUSE",   "LDINDEP", 256, "load-use interlock   (per lw whose result is used next)"},
        {"BRTAKEN", "BRNTAKN", 256, "taken-branch bubble  (per taken branch)"},
        {"DMISS",   "DHIT",    512, "cache miss           (per L1+L2-missing lw)"},
      };
      fprintf(stderr, "[verif_spike] CONTROLLED PAIRS (same instructions, cycle delta = pure stall)\n");
      fprintf(stderr, "[verif_spike] %-9s %-9s %8s %8s %9s  %s\n",
              "block_A", "block_B", "dInstr", "dCycle", "cyc/event", "isolates");
      for (auto& pp : pairs) {
        const ca_row* A = find_row(pp.a); const ca_row* B = find_row(pp.b);
        if (!A || !B) continue;
        long dcyc = (long)A->ncyc - (long)B->ncyc;
        long dins = (long)A->nins - (long)B->nins;
        fprintf(stderr, "[verif_spike] %-9s %-9s %8ld %8ld %9.2f  %s\n",
                pp.a, pp.b, dins, dcyc, (double)dcyc / (double)pp.n, pp.what);
      }
      fprintf(stderr, "[verif_spike] ------------------------------------------------------------\n");
      uint32_t cyc0 = 0, ins0 = 0;
      master_read(VERIF_CA_TABLE_ADDR + 8,  cyc0);
      master_read(VERIF_CA_TABLE_ADDR + 12, ins0);
      fprintf(stderr, "[verif_spike] host get_cycle()  = %llu  (timing model total)\n",
              (unsigned long long)get_cycle());
      fprintf(stderr, "[verif_spike] whole-run via mirror: %llu cycles / %llu instr (CPI %.3f)\n",
              (unsigned long long)(cyc_end - cyc0),
              (unsigned long long)(ins_end - ins0),
              (ins_end > ins0) ? (double)(cyc_end - cyc0) / (double)(ins_end - ins0) : 0.0);
      fprintf(stderr, "[verif_spike] model events: insn=%llu load_use=%llu br_taken=%llu "
                      "dmiss=%llu trap=%llu\n",
              (unsigned long long)timing.n_insn,
              (unsigned long long)timing.n_load_use,
              (unsigned long long)timing.n_br_taken,
              (unsigned long long)timing.n_dmiss,
              (unsigned long long)timing.n_trap);
      fprintf(stderr, "[verif_spike] supports_ca()=%d\n", (int)supports_ca());
      fprintf(stderr, "[verif_spike] CA_measure co-sim PASS (%u blocks)\n", count);
      return;
    }
  }

  // BP wait specific to CA_measure: the mailbox is in DRAM (0x80010000), so the
  // default host_wait_bp(VERIF_BREAK_ADDR=0x51000) cannot be used. Only the address differs; the logic is identical.
  bool ca_wait_bp(uint32_t bp) {
    uint32_t v = 0;
    for (long n = 0; n < 200000000L; n++) {   // bounded wait (a safety net)
      if (!master_read(VERIF_CA_BP_ADDR, v)) return false;
      if (v == bp) return true;
      tick(1);
    }
    return false;
  }
};

#endif // VERIF_SPIKE_H
