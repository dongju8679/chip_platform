#!/usr/bin/env bash
# run.sh - build and run UART_printf (chipyard backend)
#
# Two things differ from the other IP cases:
#   1) -DPRELOAD  : bp_init() in crt.S waits for the host via
#                   dut_wait_bp(BP_MEM_WRITE), but this test has no host (Vseq).
#                   PRELOAD must be on to skip the handshake and reach main().
#   2) +verif=    : not passed -> uses the standard testchip_tsi_t
#                   (verif_tsi_t is not needed).
#
# usage: ./verif/ip/S5740/UART/printf/run.sh
#        CHIPYARD=/path/to/chipyard CONFIG=RV32RocketConfig ./...
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"   # chip_platform top level

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"
TEST=UART_printf

if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  source "$CHIPYARD/env.sh" 2>/dev/null || true
  command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || \
    export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"
fi
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found (CHIPYARD=$CHIPYARD)"; exit 1; }

cd "$CP_DIR"
echo "=== [1/2] firmware build: $TEST ==="
# The Makefile does not treat EXTRA_CFLAGS changes as a dependency (docs/FreeRTOS_port.md 8.2).
rm -f "build/${TEST}.elf"
make -f Implementation/Makefile.VERIF src_dir=. F=UART SF=printf \
     TRACK=baremetal CHIPYARD="$CHIPYARD" CONFIG="$CONFIG" EXTRA_CFLAGS=-DPRELOAD

SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
[ -x "$SIM" ] || { echo "[!] simulator not built: $SIM"; exit 1; }

echo "=== [2/2] simulator run (UART0 -> stdout) ==="
# 115200 baud @ 500MHz pbus = about 43k cycles per character, so this takes minutes.
mkdir -p build/logs
stdbuf -o0 -e0 "$SIM" +permissive +permissive-off "build/${TEST}.elf" \
  2>&1 | tee "build/logs/${TEST}.log"

echo "=== check ==="
if grep -q "UART_PRINTF_DONE" "build/logs/${TEST}.log"; then
  echo "PASS: UART stdout path healthy (printf -> _write -> UART0)"
else
  echo "FAIL: UART_PRINTF_DONE was not printed"; exit 1
fi
