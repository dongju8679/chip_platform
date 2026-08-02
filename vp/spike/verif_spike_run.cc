// verif_spike_run.cc - the spike backend host main (the verification driver)
//
// The host entry point. On chipyard the host main (SimTSI.cc) lives in the
//   chipyard tree, but spike is a library (libriscv) with no main of its own, so we write it.
//   = that is why this file lives under backend/spike/.
//
// Directories called/used:
//   - backend/spike/verif_spike.h        (the adapter class this main constructs)
//   - VERIF/include/verif_primitives.h   (inherited by verif_spike.h; used transitively)
//   - Implementation/build/<TEST>.elf    (the firmware built by Makefile.VERIF; taken as an argument)
//   - riscv-isa-sim (libriscv)           (linked by verif_spike.h; externally installed)
//
// Called from:
//   - backend/spike/run.sh  ->  ./verif_spike_run <elf> +verif=<TEST>
//   - run.sh is invoked by `make run BACKEND=spike` in Implementation/Makefile.VERIF.
//
// Build: backend/spike/build.sh (g++ -I$RISCV/include ... -lriscv).

#include <cstdio>
#include <cstring>
#include <string>
#include "verif_spike.h"

int main(int argc, char** argv) {
  // -- 1) Argument parsing --
  //   argv: <elf_path> [+verif=<TEST>] [+isa=<isa>] [+tohost=<hex>]
  std::string elf_path, test, isa = "rv32imac_zicsr_zifencei";
  uint64_t tohost_addr = 0;
  int tick_chunk = 1000;   // step granularity (instructions to advance at once)

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if      (a.rfind("+verif=",  0) == 0) test = a.substr(7);
    else if (a.rfind("+isa=",    0) == 0) isa  = a.substr(5);
    else if (a.rfind("+tohost=", 0) == 0) tohost_addr = strtoull(a.substr(8).c_str(), nullptr, 0);
    else if (a.rfind("+chunk=",  0) == 0) tick_chunk = atoi(a.substr(7).c_str());
    else if (a[0] != '+')                 elf_path = a;   // the first non-flag argument = the ELF
  }
  if (elf_path.empty()) {
    fprintf(stderr, "usage: verif_spike_run <elf> [+verif=<TEST>] [+isa=<isa>] [+tohost=<hex>]\n");
    return 2;
  }

  // -- 2) Adapter construction (sim_t/processor_t are built inside verif_spike.h) --
  verif_spike_t v(isa);

  // -- 3) ELF loading (PC=_start, obtain the tohost symbol) --
  if (!v.load_elf(elf_path)) {
    fprintf(stderr, "[verif_spike_run] load_elf failed: %s\n", elf_path.c_str());
    return 2;
  }
  if (tohost_addr) v.set_tohost(tohost_addr);   // when explicitly given (otherwise load_elf obtains it)

  // -- 4) Test selection (for the run_vseq dispatch) --
  v.set_test(test);
  fprintf(stderr, "[verif_spike_run] running test='%s' elf=%s\n",
          test.empty() ? "(self-check)" : test.c_str(), elf_path.c_str());

  // -- 5) Verification loop --  (exactly the sequence in spike_call_sequence.md)
  //   while(!done) { tick -> idle -> poll done }
  int guard = 0, guard_max = 100000000;   // infinite-loop guard (a safety net)
  while (!v.done()) {
    v.tick(tick_chunk);     // advance the firmware by N instructions (proc->step)
    v.idle();               // the host's turn (dispatches run_vseq on the first call)
    if (++guard > guard_max) {
      fprintf(stderr, "[verif_spike_run] TIMEOUT (no tohost)\n");
      return 3;
    }
  }

  // -- 6-8) PASS/FAIL verdict --  tohost >> 1
  int code = v.exit_code();
  bool pass = (code == 0);
  printf("[verif_spike_run] %s (test=%s, exit_code=%d)\n",
         pass ? "PASS" : "FAIL", test.c_str(), code);

  // Process exit code: PASS=0, FAIL=1 (used by CI for the verdict)
  return pass ? 0 : 1;
}
