#!/usr/bin/env bash
# regress.sh - regression runner (only ever run against a dedicated chipyard copy)
#
# Purpose: check in one go that HAL cleanup work did not break existing functionality.
#
# * Safety rule
#   This script aborts immediately if CHIPYARD points at a pristine tree.
#
# usage:
#   ./verif/regress.sh                 # fast set
#   ./verif/regress.sh full            # everything, including benchmarks / FreeRTOS spec
#   ./verif/regress.sh <name> [...]    # individual cases only
#
# Case names (times are rough RTL estimates):
#   clint_sw      CLINT sw_interrupt   (co-sim, +verif)      ~1 min
#   clint_timer   CLINT timer_interrupt(co-sim, +verif)      does NOT finish on RTL
#   exception     EXCEPTION traps      (self-check)          ~1.5 min
#   pmp           PMP violation traps  (self-check)          ~2 min
#   uart          UART printf          (stdout)              ~75 min
#   ca            CA measure           (co-sim, +verif)      ~6 min
#   spike_ca      CA measure on spike  (vp/spike)            ~1 s
#   freertos_fast FreeRTOS preempt (50kHz tick)              ~17 min
#   freertos      FreeRTOS preempt (1kHz tick, spec)         ~50 min
#   coremark      CoreMark                                   ~10 min
#   dhrystone     Dhrystone                                  ~9 min
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CP_DIR"

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"

# -- safety guard: refuse pristine trees --------------------------------
# Use the dedicated copy's toolchain too. The shell profile puts ~/chipyard on PATH,
# so left alone the pristine tree's gcc gets picked up. That is read-only and not
# dangerous, but pin it anyway for reproducibility.
[ -d "$CHIPYARD/.conda-env/riscv-tools/bin" ] && \
  export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found (CHIPYARD=$CHIPYARD)"; exit 1; }

SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
[ -x "$SIM" ] || { echo "[!] simulator not built: $SIM"; exit 1; }

export CHIPYARD CONFIG   # child run.sh scripts inherit these (avoids their pristine-tree defaults)

mkdir -p build/logs
MK="make -f Implementation/Makefile.VERIF src_dir=. CHIPYARD=$CHIPYARD CONFIG=$CONFIG"

PASSED=(); FAILED=()
report() { # report <name> <0|1>
  if [ "$2" = "0" ]; then PASSED+=("$1"); echo "  ==> PASS: $1";
  else FAILED+=("$1"); echo "  ==> FAIL: $1"; fi
}

# -- simulator run helper ----------------------------------------------
# In co-sim cases the DUT can keep spinning inside dut_wait_bp() even after the host
# Vseq has printed PASS (no $finish is reached). So run it as "quit as soon as the
# verdict marker appears in the log". If the marker never appears, wait for timeout.
sim_until_marker() { # sim_until_marker <log> <marker-regex> <timeout-s> <sim-args...>
  local log=$1 marker=$2 tmo=$3; shift 3
  : > "$log"
  stdbuf -o0 -e0 "$SIM" "$@" > "$log" 2>&1 &
  local pid=$!
  local waited=0
  while kill -0 "$pid" 2>/dev/null; do
    if grep -qE "$marker" "$log"; then sleep 1; break; fi
    [ "$waited" -ge "$tmo" ] && { echo "[!] timeout ${tmo}s"; break; }
    sleep 2; waited=$((waited+2))
  done
  # * SIGTERM alone sometimes does not kill the verilator sim (an orphan process once
  #   survived and kept burning CPU). If it is still alive after TERM, send KILL.
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null
    local g=0
    while kill -0 "$pid" 2>/dev/null && [ "$g" -lt 5 ]; do sleep 1; g=$((g+1)); done
    kill -9 "$pid" 2>/dev/null
  fi
  wait "$pid" 2>/dev/null
  cat "$log"
}

# shared: co-sim cases (the host Vseq decides the verdict, uses +verif)
run_cosim() { # run_cosim <TEST> <F> <SF> [extra make args...]
  local test=$1 f=$2 sf=$3; shift 3
  echo "=== [$test] build ==="
  rm -f "build/${test}.elf"
  # co-sim cases are built with -DPRELOAD too (same as self-check).
  #   verif_host.h unconditionally does `#define PRELOAD 1` right before including the
  #   Vseq files, so the host never sends host_set_bp(BP_MEM_WRITE) for any test.
  #   If only the firmware is built without -DPRELOAD, bp_init() waits for a write that
  #   never comes, and it only gets through "by accident" because dut_wait_bp() also
  #   accepts BP_TEST_begin - that is a race with bp_init(), not a handshake.
  #   See chip_docs/verif/docs/verif_host_refactor.md section 5.
  $MK F="$f" SF="$sf" TRACK=baremetal EXTRA_CFLAGS="-DPRELOAD${EXTRA:+ $EXTRA}" "$@" \
      || { report "$test" 1; return; }
  echo "=== [$test] run (+verif=$test) ==="
  local log="build/logs/${test}.log"
  sim_until_marker "$log" "co-sim (PASS|FAIL)" "${TIMEOUT:-3600}" \
      +permissive "+verif=$test" +permissive-off "build/${test}.elf"
  if grep -q "co-sim PASS" "$log"; then report "$test" 0; else report "$test" 1; fi
}

# shared: self-check cases (no host; the DUT decides for itself and exits via tohost)
run_selfcheck() { # run_selfcheck <TEST> <F> <SF> <grep-pattern> [extra make args...]
  local test=$1 f=$2 sf=$3 pat=$4; shift 4
  echo "=== [$test] build ==="
  rm -f "build/${test}.elf"
  $MK F="$f" SF="$sf" TRACK=baremetal EXTRA_CFLAGS="-DPRELOAD${EXTRA:+ $EXTRA}" "$@" \
      || { report "$test" 1; return; }
  echo "=== [$test] run ==="
  local log="build/logs/${test}.log"
  sim_until_marker "$log" "${pat}|_FAIL" "${TIMEOUT:-3600}" \
      +permissive +permissive-off "build/${test}.elf"
  if grep -q "$pat" "$log"; then report "$test" 0; else report "$test" 1; fi
}

do_case() {
case "$1" in
  clint_sw)      run_cosim CLINT_sw_interrupt CLINT sw_interrupt ;;
  clint_timer)   run_cosim CLINT_timer_interrupt CLINT timer_interrupt ;;
  exception)     run_selfcheck EXCEPTION_traps EXCEPTION traps "EXCEPTION_TRAPS_PASS" ;;
  pmp)           run_selfcheck PMP_violation PMP violation "PMP_VIOLATION_PASS" ;;
  uart)          verif/ip/S5740/UART/printf/run.sh && report uart 0 || report uart 1 ;;
  ca)            verif/ip/S5740/CA/measure/run.sh && report ca 0 || report ca 1 ;;
  spike_ca)      spike_ca_run ;;
  freertos_fast) MODE=fast verif/ip/S5740/FREERTOS/preempt/run.sh && report freertos_fast 0 || report freertos_fast 1 ;;
  freertos)      verif/ip/S5740/FREERTOS/preempt/run.sh && report freertos 0 || report freertos 1 ;;
  coremark)      verif/ip/S5740/BENCH/coremark/run.sh && report coremark 0 || report coremark 1 ;;
  dhrystone)     verif/ip/S5740/BENCH/dhrystone/run.sh && report dhrystone 0 || report dhrystone 1 ;;
  *) echo "[!] unknown case: $1"; exit 2;;
esac
}

# spike backend: run the CA_measure ELF through vp/spike (verif_spike_run)
spike_ca_run() {
  local test=CA_measure
  [ -f "build/${test}.elf" ] || { echo "  (build/${test}.elf missing -> run ca first)"; report spike_ca 1; return; }
  [ -x vp/spike/verif_spike_run ] || { echo "  (vp/spike/verif_spike_run not found)"; report spike_ca 1; return; }
  local log="build/logs/spike_${test}.log"
  vp/spike/verif_spike_run "build/${test}.elf" "+verif=$test" 2>&1 | tee "$log"
  grep -q "PASS" "$log" && report spike_ca 0 || report spike_ca 1
}

FAST=(clint_sw clint_timer exception pmp uart ca spike_ca freertos_fast)
FULL=(clint_sw clint_timer exception pmp uart ca spike_ca freertos coremark dhrystone)

if [ $# -eq 0 ]; then      CASES=("${FAST[@]}")
elif [ "$1" = "full" ]; then CASES=("${FULL[@]}")
elif [ "$1" = "fast" ]; then CASES=("${FAST[@]}")
else                        CASES=("$@"); fi

for c in "${CASES[@]}"; do do_case "$c"; done

echo
echo "================ regression summary ================"
echo "PASS (${#PASSED[@]}): ${PASSED[*]:-none}"
echo "FAIL (${#FAILED[@]}): ${FAILED[*]:-none}"
[ ${#FAILED[@]} -eq 0 ]
