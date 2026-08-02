#!/usr/bin/env bash
# run.sh - build and run CoreMark (chipyard verilator backend)
#
# Follows the UART_printf/run.sh pattern. Differences:
#   1) USE_BENCH=coremark : adds Middleware/benchmark to the build and replaces
#                           printf.c with bench_stdio.c (HTIF console).
#   2) -DPRELOAD          : makes bp_init() in crt.S skip the host handshake.
#                           (without it, execution stalls before main())
#   3) no +verif=         : uses the standard testchip_tsi_t. Results come out
#                           on the HTIF console.
#
# usage: ./verif/ip/S5740/BENCH/coremark/run.sh
#        ITERATIONS=5 CHIPYARD=/path/to/chipyard CONFIG=RV32RocketConfig ./...
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"
OPT="${OPT:--O2}"
# RTL (verilator) runs at a few thousand cycles/s. One CoreMark iteration is
# roughly a few hundred thousand cycles, so raising the iteration count grows
# wall-time linearly. Default: 2.
ITERATIONS="${ITERATIONS:-2}"
TIMEOUT="${TIMEOUT:-14400}"
TEST=BENCH_coremark

if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  set +u; source "$CHIPYARD/env.sh" 2>/dev/null || true; set -u
  command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || \
    export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"
fi
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found (CHIPYARD=$CHIPYARD)"; exit 1; }

cd "$CP_DIR"
mkdir -p build/logs

echo "=== [1/2] firmware build: $TEST (ITERATIONS=$ITERATIONS, OPT=$OPT) ==="
rm -f "build/${TEST}.elf"
make -f Implementation/Makefile.VERIF src_dir=. F=BENCH SF=coremark \
     TRACK=baremetal USE_BENCH=coremark BENCH_ITERATIONS="$ITERATIONS" \
     OPT="$OPT" CHIPYARD="$CHIPYARD" CONFIG="$CONFIG" \
     EXTRA_CFLAGS=-DPRELOAD

echo "--- size check (link.ld RAM = 60K) ---"
riscv64-unknown-elf-size "build/${TEST}.elf"

SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
[ -x "$SIM" ] || { echo "[!] simulator not built: $SIM"; exit 1; }

echo "=== [2/2] simulator run (HTIF console -> stdout) ==="
START=$(date +%s)
timeout "$TIMEOUT" stdbuf -o0 -e0 "$SIM" +permissive +permissive-off \
  "build/${TEST}.elf" 2>&1 | tee "build/logs/${TEST}.log" || true
END=$(date +%s)
echo "wall-time: $((END-START)) s" | tee -a "build/logs/${TEST}.log"

echo "=== check ==="
if grep -q "BENCH_COREMARK_DONE" "build/logs/${TEST}.log"; then
  grep -E "^\[bench\]|^\[CA\]|^CoreMark|^Total|^Iterations|^seedcrc|crc|^Correct|^Errors" \
       "build/logs/${TEST}.log" || true
  echo "PASS: CoreMark ran to completion"
else
  echo "FAIL: BENCH_COREMARK_DONE was not printed (check build/logs/${TEST}.log)"; exit 1
fi
