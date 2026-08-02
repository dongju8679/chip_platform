// verif_host.h - chipyard backend adapter (chipyard impl of verif_primitives_t)
//
// Two jobs, and nothing else:
//   1. implement the framework seam (verif_primitives.h) on top of chipyard testchip_tsi_t (TSI)
//        master_write/read -> write_chunk/read_chunk
//        tick              -> switch_to_target() (1 clock per call)
//        trigger_irq       -> sim_ext_intr_trigger() (real only when the CONFIG's HarnessBinder
//                             pulls in SimExtInterrupts.cc - see apply_backend.sh)
//   2. dispatch +verif=<TEST> to that test's company Vseq (see run_vseq at the bottom).
//
// * What does NOT belong here: the host sequences themselves. They are the company originals in
//   verif/ip/S5740/<IP>/<sub>/Vseq/<test>.cc, included above the class and called by name. Before
//   v304 this file re-implemented each of them by hand, which meant two sources of truth for the
//   same protocol; the two only agreed by accident (PLIC_enable ran 8 lines here vs the DUT's 45).
//   They compile as-is now that verif_addr_chipyard.h defines BREAK_POINT_ADDR and
//   S5740_mcu_tests.h forwards host_check_bp / matches rt-dev's return conventions.
//
// Built in the chipyard tree (testchipip csrc). SimTSI.cc creates verif_tsi_t on +verif.
// Build pitfalls/application are handled by backend/chipyard/apply_backend.sh.
#ifndef VERIF_HOST_H
#define VERIF_HOST_H

#include <cstdint>
#include <cstdio>
#include <string>
#include "testchip_tsi.h"
#include "testchip_external_interrupts.h"

// -- chipyard DUT address map --
// All of it now lives in one place (verif_addr_chipyard.h) so the host and the company Vseq
// files agree on it without either side redefining anything. It must be included before
// verif_primitives.h, whose VERIF_TEST_ADDR/VERIF_BREAK_ADDR defaults are #ifndef-guarded.
#include "verif_addr_chipyard.h"
#include "verif_primitives.h"   // seam (via -I from VERIF/include). BP_* numbers are common.
#include <cstdlib>
#include <ctime>

// -- company Vseq wiring --
// g_verif is what S5740_mcu_tests.h's free functions forward to; idle() sets it before dispatch.
// Including the Vseq .cc here is what lets the company test files stay byte-identical:
// they call master_write()/tick()/... as free functions, exactly as on rt-dev.
#include "S5740_mcu_tests.h"
verif_primitives_t* g_verif = nullptr;

// NOTE: one poll here is master_read (a multi-cycle TSI read_chunk) + tick(1), so this bound is
// worth hundreds of millions of simulation cycles -- in practice max-cycles fires long first and
// the timeout branch below never runs. Kept at the original value; lower it (100000 works) if you
// need the diagnostic to actually print.
#define VERIF_BP_TIMEOUT   (2000000L)      // host_wait_bp bound (ticks) before reporting
#define VERIF_DUT_READY_TIMEOUT (50000L)   // wait_dut_ready bound: polls for bp_init()'s BP_TEST_init
                                           // (same bound the pre-refactor CLINT_timer block used)

// The company Vseq files guard the memory-write handshake with #ifndef PRELOAD. On this backend
// fesvr(testchip_tsi_t) loads the ELF into DRAM itself, which *is* the preload - the DUT skips
// the handshake too (crt.S -> bp_init(), built with -DPRELOAD). Saying so here keeps host and DUT
// on the same protocol; without it the host would open with a stray BP_MEM_WRITE that the DUT is
// not waiting for, and race its bp_init().
#define PRELOAD 1

// -- company original Vseq, one #include per test --------------------------------
// This is the whole point of the v304 refactor: the sequences are the company originals under
// verif/ip/S5740/<IP>/<sub>/Vseq/, not a second implementation living in this header.
// apply_backend.sh copies each Vseq .cc and its Inc/<test>.h next to SimTSI.cc, so they resolve
// through the same -I as the rest of the harness sources. verif_vseq_reset.h drops the previous
// test's per-test macros in between (see that file for why).
#include "I2SR_int_muldiv.cc"
#include "verif_vseq_reset.h"
#include "CLINT_rtc.cc"
#include "verif_vseq_reset.h"
#include "CLINT_sw_interrupt.cc"
#include "verif_vseq_reset.h"
#include "CLINT_timer_interrupt.cc"
#include "verif_vseq_reset.h"
#include "PLIC_enable.cc"
#include "verif_vseq_reset.h"
#include "PLIC_latency.cc"
#include "verif_vseq_reset.h"
#include "CA_measure.cc"
#include "verif_vseq_reset.h"
#include "DEMO_handshake.cc"


// verif_tsi_t = chipyard adapter. testchip_tsi_t (execution engine) + verif_primitives_t (interface).
class verif_tsi_t : public testchip_tsi_t, public verif_primitives_t {
public:
  verif_tsi_t(int argc, char** argv) : testchip_tsi_t(argc, argv, true) {
    ran = false;
    fprintf(stderr, "[verif_tsi] ACTIVE (chipyard backend)\n");
    for (int i = 0; i < argc; i++) {
      std::string a = argv[i];
      if (a.rfind("+verif=", 0) == 0) test = a.substr(7);
    }
  }

  // -- seam implementation (verif_primitives_t) --
  bool master_write(uint32_t addr, uint32_t data) override {
    write_chunk((addr_t)addr, sizeof(uint32_t), &data);
    return true;
  }
  bool master_read(uint32_t addr, uint32_t& data) override {
    read_chunk((addr_t)addr, sizeof(uint32_t), &data);
    return true;
  }
  void trigger_irq(uint32_t mask) override {
      sim_ext_intr_trigger(0, mask);  
  }
  void todo_change(uint32_t mask) {
    // chipyard interrupt injection (DRAM mirror mailbox method - no DPI).
    //   Uses a cooperative model (soft-IRQ) where the DUT polls IRQ_INJECT_ADDR.
    //   When the host writes a mask here, the DUT polling routine raises the corresponding IRQ.
    //   (Real HW interrupts need HarnessBinder/DPI, but this cooperative model is
    //    enough for co-sim - can measure PLIC routing/priority/latency function and cycles.)
    master_write(VERIF_IRQ_INJECT_ADDR, mask);
    // Explicitly give the DUT(target) time to poll/handle the ISR after IRQ injection.
    // (Advance the target with master_read rather than relying on fprintf I/O yield.
    //  read_chunk advances the target when called, so repeat enough for the DUT to
    //  receive the IRQ, run the ISR, and set BP to END. 200 iterations is a measured PASS value.)
    uint32_t dummy;
    for (int i = 0; i < 200; i++) master_read(VERIF_IRQ_INJECT_ADDR, dummy);
  }
  void tick(int n) override {
    // fesvr(tsi_t) is coroutine-based: switch_to_target() yields to the target context,
    // which runs until the next tsi_tick() DPI call from SimTSI.v - i.e. exactly 1 clock.
    // So tick(n) advances n clocks, equivalent to rt-dev's emulator tick(n).
    for (int i = 0; i < n; i++) switch_to_target();
  }
  // -- host BP helper override --
  // The verif_primitives_t default calls tick(1) inside host_wait_bp, but this backend's
  // tick is a no-op, so the DUT(target) does not advance and it waits forever (timeout).
  // On the chipyard backend the target advances when master_read(=read_chunk) is called,
  // so override to repeat master_read without tick. (Form verified PASS in diagnostics)
  void host_set_bp(uint32_t bp) override {
    master_write(VERIF_BREAK_ADDR, bp);
  }
  bool host_wait_bp(uint32_t bp) override {
    uint32_t v = 0;
    // Bounded wait: a hang here means the DUT never reached the expected step.
    // On timeout, report what the DUT actually left behind so the failure is diagnosable
    // instead of an infinite spin.
    for (long n = 0; n < VERIF_BP_TIMEOUT; n++) {
      if (!master_read(VERIF_BREAK_ADDR, v)) return false;
      if (v == bp) return true;
      tick(1);
    }
    uint32_t dbg = 0;
    master_read(VERIF_DUT_DBG_ADDR, dbg);
    fprintf(stderr,
            "[verif] TIMEOUT waiting bp=%u : bp_addr(0x%08x)=%u  dut_dbg(0x%08x)=%u\n",
            bp, VERIF_BREAK_ADDR, v, VERIF_DUT_DBG_ADDR, dbg);
    fprintf(stderr, "[verif]   dut_dbg meaning: 1=main 2=I2SR_EN 3=clint 4=plic_thr 5=reg_ext "
                    "6=reg_hnd 7=mtvec/mie 8=wait_bp1 9=set_bp2 10=loop_in 11=wait_bp3 12=cnt_ok\n");
    return false;
  }
  bool host_check_bp(uint32_t addr, uint32_t expect) override {
    uint32_t v;
    if (!master_read(addr, v)) return false;
    return v == expect;
  }

  // -- CA extension --
  // chipyard is RTL/Verilator = cycle-accurate. supports_ca()=true.
  //   get_cycle(): read the DUT mcycle mirror (VERIF_CYCLE_LO/HI_ADDR).
  //     Assumes the DUT-side hal_shim updates mcycle at that address (no DPI).
  uint64_t get_cycle() override {
    uint32_t lo = 0, hi = 0;
    master_read(VERIF_CYCLE_LO_ADDR, lo);
    master_read(VERIF_CYCLE_HI_ADDR, hi);
    return ((uint64_t)hi << 32) | lo;
  }
  bool supports_ca() override { return true; }    // chipyard supports CA

  // Companion to get_cycle(): committed-instruction count (minstret) mirror.
  // Cycles alone don't prove anything; CPI = dcycle/dinstret is what shows the stalls.
  uint64_t get_instret() {
    uint32_t lo = 0, hi = 0;
    master_read(VERIF_INSTR_LO_ADDR, lo);
    master_read(VERIF_INSTR_HI_ADDR, hi);
    return ((uint64_t)hi << 32) | lo;
  }

  // -- chipyard host context entry point --
  void idle() override {
    if (ran) { testchip_tsi_t::idle(); return; }
    ran = true;
    if (test.empty()) return;      // no +verif= : plain run, host stays out (no barrier, no vseq)
    wait_dut_ready();
    run_vseq();
  }

private:
  bool ran;
  std::string test;

  // -- DUT boot barrier (must run before any host sequence) ------------------------
  // idle() first fires while the DUT is still in crt.S, long before it reaches main().
  // crt.S calls bp_init(), which writes BP_TEST_init to BREAK_POINT_ADDR unconditionally.
  // A sequence that opens with host_set_bp(BP_TEST_begin) therefore has its value clobbered
  // by bp_init(), and the DUT then waits in dut_wait_bp(BP_TEST_begin) for a begin that
  // already came and went -- host waits for END, DUT waits for begin, nothing is printed.
  // That is a backend fact (fesvr preloads DRAM, then the core boots), not something the
  // company Vseq should have to know about, so the barrier lives here and every Vseq under
  // verif/ip/.../Vseq/ stays byte-identical to rt-dev.
  void wait_dut_ready() {
    uint32_t v = 0;
    for (long n = 0; n < VERIF_DUT_READY_TIMEOUT; n++) {
      if (!master_read(VERIF_BREAK_ADDR, v)) break;
      if (v == BP_TEST_init) return;                 // bp_init() done -> DUT is in main()
      tick(1);
    }
    fprintf(stderr, "[verif] WARN: DUT not ready after %ld polls "
                    "(bp_addr(0x%08x)=0x%08x, expected BP_TEST_init=0x%08x) - running anyway\n",
            (long)VERIF_DUT_READY_TIMEOUT, VERIF_BREAK_ADDR, v, (uint32_t)BP_TEST_init);
  }

  // -- dispatch only ---------------------------------------------------------------
  // One row per co-sim test: the name that follows +verif= and the company Vseq entry point.
  // No host sequence logic lives in this file any more - adding a test is one #include above
  // plus one row here.
  void run_vseq() {
    if (test.empty()) return;                 // no +verif= : plain self-check run, host stays out

    struct vseq_entry { const char* name; bool (*run)(); };
    static const vseq_entry table[] = {
      { "I2SR_int_muldiv",       &::I2SR_int_muldiv       },
      { "CLINT_rtc",             &::CLINT_rtc             },
      { "CLINT_sw_interrupt",    &::CLINT_sw_interrupt    },
      { "CLINT_timer_interrupt", &::CLINT_timer_interrupt },
      { "PLIC_enable",           &::PLIC_enable           },
      { "PLIC_latency",          &::PLIC_latency          },
      { "CA_measure",            &::CA_measure            },
      { "DEMO_handshake",        &::DEMO_handshake        },
    };

    for (const vseq_entry& e : table) {
      if (test != e.name) continue;
      g_verif = this;                         // the Vseq free functions forward here
      bool ok = e.run();
      fprintf(stderr, "[verif] %s co-sim %s\n", e.name, ok ? "PASS" : "FAIL");
      return;
    }
    fprintf(stderr, "[verif] no Vseq registered for '%s' - host does nothing.\n", test.c_str());
  }
};

// Declared in S5740_mcu_tests.h so a Vseq can call it; defined here because it needs the
// complete verif_tsi_t. get_instret() is not on the seam and the seam is not ours to extend.
// (One definition, one TU - verif_host.h is included only by SimTSI.cc.)
uint64_t host_get_instret() { return static_cast<verif_tsi_t*>(g_verif)->get_instret(); }

#endif // VERIF_HOST_H
