// verif_ca_run.cc - the Cycle-Accurate (CA) verification host main (a cycle measurement driver)
//
// The CA verification entry point. A separate driver from functional verification (verif_spike_run.cc).
//   Measures "in how many cycles does it happen" and decides PASS/FAIL against an expected value +- a tolerance.
//
// Where each function lives in the verification flow:
//   main()                         <- this file (backend/spike/verif_ca_run.cc)
//   |- verif_spike_t (or verif_tsi_t)     <- backend/<B>/verif_spike.h / verif_host.h
//   |- supports_ca()/get_cycle()   <- the backend header (the CA extension implementation)
//   |- ca_pass()                   <- VERIF/include/verif_primitives.h (the shared verdict)
//   `- host_wait_bp()/trigger_irq()/master_read()  <- the backend header (the seam implementation)
//
// Build: add this target to backend/spike/build.sh (a sibling of verif_spike_run).
// Run: run.sh ... +mode=ca +expected=<cyc> +tolerance=<cyc>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include "verif_spike.h"     // or the per-backend header. ca_measurement_t/ca_pass come in through this.

int main(int argc, char** argv) {
  // -- 1) Argument parsing --
  std::string elf_path, test, isa = "rv32imac_zicsr_zifencei";
  uint64_t expected = 0, tolerance = 0, tohost_addr = 0;
  bool use_pmu = false;          // true = path A (PMU), false = path B (host get_cycle)
  uint32_t pmu_addr = 0;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if      (a.rfind("+verif=",     0) == 0) test = a.substr(7);
    else if (a.rfind("+isa=",       0) == 0) isa  = a.substr(5);
    else if (a.rfind("+expected=",  0) == 0) expected  = strtoull(a.substr(10).c_str(), 0, 0);
    else if (a.rfind("+tolerance=", 0) == 0) tolerance = strtoull(a.substr(11).c_str(), 0, 0);
    else if (a.rfind("+tohost=",    0) == 0) tohost_addr = strtoull(a.substr(8).c_str(), 0, 0);
    else if (a.rfind("+pmu=",       0) == 0) { use_pmu = true; pmu_addr = strtoul(a.substr(5).c_str(), 0, 0); }
    else if (a[0] != '+')                    elf_path = a;
  }
  if (elf_path.empty()) {
    fprintf(stderr, "usage: verif_ca_run <elf> +verif=<TEST> +expected=<cyc> +tolerance=<cyc> [+pmu=<addr>]\n");
    return 2;
  }

  // -- 2) Backend construction + ELF loading --
  verif_spike_t b(isa);              // (on chipyard this would be verif_tsi_t - same interface)
  b.load_elf(elf_path);
  if (tohost_addr) b.set_tohost(tohost_addr);
  b.set_test(test);

  // -- 3) Check whether the backend supports CA (otherwise SKIP) --
  //   supports_ca(): vp/spike/verif_spike.h=true (the C timing model) /
  //                  backend/chipyard/verif_host.h=true (RTL) /
  //                  backend/rt-dev/verif_rtdev.h=true (RTL).
  //   All three currently support CA -> SKIP never triggers. This is a safety net for when a non-CA backend is added.
  if (!b.supports_ca()) {
    printf("[verif_ca_run] SKIP test=%s (backend not cycle-accurate)\n", test.c_str());
    return 77;                       // 77 = SKIP (the automake convention)
  }

  // -- 4) CA measurement (run_ca) --
  ca_measurement_t ca;
  ca.name      = test.c_str();
  ca.expected  = expected;
  ca.tolerance = tolerance;

  // (a) Wait for the firmware to be ready - host_wait_bp: the backend header (the verif_primitives_t default implementation)
  if (!b.host_wait_bp(BP_TEST_begin)) {
    fprintf(stderr, "[verif_ca_run] FAIL wait begin\n");
    return 1;
  }

  // (b) Start measuring - get_cycle: the backend header (on chipyard, a read of the mcycle mirror)
  ca.cycle_start = b.get_cycle();

  // (c) Stimulus - trigger_irq: the backend header (spike sets mip / chipyard uses stage3)
  b.trigger_irq(/*irq_id=*/3);

  // (d) Wait for the ISR to complete
  if (!b.host_wait_bp(BP_TEST_end)) {
    fprintf(stderr, "[verif_ca_run] FAIL wait end\n");
    return 1;
  }

  // (e) End of measurement
  ca.cycle_end = b.get_cycle();

  // (f) Compute the measured latency - two paths
  if (use_pmu) {
    // Path A: the value the firmware measured with the PMU (a DUT hardware counter, the PLIC_latency approach)
    uint32_t pmu = 0; b.master_read(pmu_addr, pmu);
    ca.measured = pmu;
  } else {
    // Path B: the host-side cycle difference
    ca.measured = ca.cycle_end - ca.cycle_start;
  }

  // -- 5) PASS/FAIL verdict -- ca_pass: VERIF/include/verif_primitives.h (shared)
  bool pass = ca_pass(ca);

  // -- 6) Reporting --
  printf("[verif_ca_run] %s: measured=%llu expected=%llu +/-%llu -> %s\n",
         ca.name,
         (unsigned long long)ca.measured,
         (unsigned long long)ca.expected,
         (unsigned long long)ca.tolerance,
         pass ? "PASS" : "FAIL");

  return pass ? 0 : 1;
}
