#!/usr/bin/env bash
# run.sh - build and run Dhrystone 2.1 (chipyard verilator backend)
#
# Follows the UART_printf/run.sh pattern. Differences:
#   1) USE_BENCH=dhrystone : adds Middleware/benchmark to the build and replaces
#                            printf.c with bench_stdio.c (HTIF console).
#   2) -DPRELOAD           : makes bp_init() in crt.S skip the host handshake.
#   3) The iteration count is the original NUMBER_OF_RUNS(=500) from
#      dhrystone.h. The original defines it without an #ifndef guard, so
#      overriding it with -D produces a redefinition warning and the header
#      value wins anyway -> we chose not to touch the official default.
#      500 runs x a few hundred cycles is a few hundred thousand cycles, which
#      finishes in minutes even on RTL.
#
# usage: ./verif/ip/S5740/BENCH/dhrystone/run.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"
OPT="${OPT:--O2}"
TIMEOUT="${TIMEOUT:-14400}"
TEST=BENCH_dhrystone

if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  set +u; source "$CHIPYARD/env.sh" 2>/dev/null || true; set -u
  command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || \
    export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"
fi
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found (CHIPYARD=$CHIPYARD)"; exit 1; }

cd "$CP_DIR"
mkdir -p build/logs

echo "=== [1/2] firmware build: $TEST (OPT=$OPT) ==="
rm -f "build/${TEST}.elf" build/dhrystone_main.o
make -f Implementation/Makefile.VERIF src_dir=. F=BENCH SF=dhrystone \
     TRACK=baremetal USE_BENCH=dhrystone \
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
if grep -q "BENCH_DHRYSTONE_DONE" "build/logs/${TEST}.log"; then
  grep -E "^\[bench\]|^\[CA\]|^Microseconds|^Dhrystones" "build/logs/${TEST}.log" || true
  echo "PASS: Dhrystone ran to completion"
else
  echo "FAIL: BENCH_DHRYSTONE_DONE was not printed (check build/logs/${TEST}.log)"; exit 1
fi
