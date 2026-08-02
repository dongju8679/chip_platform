#!/usr/bin/env bash
# run.sh - build and run CLINT_sw_interrupt (chipyard backend, co-sim)
#
# This case is a co-sim where the host Vseq must poke MSIP directly. It cannot
# be verified on spike (fast) and is RTL-only - which is why
# ci/run_regress.sh full comes here.
# (Before this script existed, full mode fell through to SKIP(no-runner).)
#
# Differences from the other cases:
#   - +verif=CLINT_sw_interrupt is passed -> SimTSI.cc constructs a verif_tsi_t
#     and the dispatch in verif_host.h runs Vseq/CLINT_sw_interrupt.cc.
#   - -DPRELOAD is required: fesvr (testchip_tsi_t) has already loaded the ELF
#     into DRAM, so the host never issues BP_MEM_WRITE. Without it, bp_init() in
#     crt.S spins forever inside dut_wait_bp(BP_MEM_WRITE).
#   - The PASS verdict is the "[verif] CLINT_sw_interrupt co-sim PASS" line
#     printed by verif_host.h.
#
# usage: ./verif/ip/S5740/CLINT/sw_interrupt/run.sh
#        CHIPYARD=/path CONFIG=RV32RocketConfig ./...
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"   # chip_platform top level

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
# -- Safety guard --------------------------------------------------
CONFIG="${CONFIG:-RV32RocketConfig}"
MAX_CYCLES="${MAX_CYCLES:-20000000}"
TEST=CLINT_sw_interrupt

# Avoid clashing with set -u: env.sh may touch undefined variables, so disable it briefly
if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  set +u
  source "$CHIPYARD/env.sh" 2>/dev/null || true
  set -u
  command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || \
    export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"
fi
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found (CHIPYARD=$CHIPYARD)"; exit 1; }

cd "$CP_DIR"
echo "=== [1/2] firmware build: $TEST ==="
# The Makefile does not treat EXTRA_CFLAGS changes as a dependency -> always delete and rebuild.
rm -f "build/${TEST}.elf"
make -f Implementation/Makefile.VERIF src_dir=. F=CLINT SF=sw_interrupt \
     TRACK=baremetal CHIPYARD="$CHIPYARD" CONFIG="$CONFIG" EXTRA_CFLAGS=-DPRELOAD

SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
[ -x "$SIM" ] || { echo "[!] simulator not built: $SIM"; exit 1; }

echo "=== [2/2] simulator run: +verif=$TEST ==="
mkdir -p build/logs
LOG="build/logs/${TEST}.log"
stdbuf -o0 -e0 "$SIM" +permissive +verif="$TEST" +max-cycles="$MAX_CYCLES" \
  +permissive-off "build/${TEST}.elf" 2>&1 | tee "$LOG"

echo "=== check ==="
if grep -q "\[verif\] CLINT_sw_interrupt co-sim PASS" "$LOG"; then
  echo "PASS: host MSIP injection -> DUT sw interrupt handling confirmed"
else
  echo "FAIL: co-sim PASS marker missing (check $LOG)"; exit 1
fi
