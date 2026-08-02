#!/usr/bin/env bash
# run.sh - build and run PMP_violation (chipyard backend, self-check)
#
# Follows the UART_printf/run.sh pattern. Differences:
#   - Output goes to the HTIF console, not the UART (Platform/Common/Inc/htif.h).
#     The UART costs 43k cycles per character, so a single marker line takes minutes.
#   - -DPRELOAD is required: there is no host (Vseq), so the bp_init() handshake
#                      in crt.S must be skipped to reach main().
#   - no +verif=     : uses the standard testchip_tsi_t (the HTIF syscall is needed).
#
# Safety rule: never run inside the pristine chipyard (~/chipyard,
#
# usage: ./verif/ip/S5740/PMP/violation/run.sh
#        CHIPYARD=/path CONFIG=RV32RocketConfig ./...
#        BACKEND=spike ./...        # quick functional check on spike
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"
BACKEND="${BACKEND:-chipyard}"
TEST=PMP_violation

[ -d "$CHIPYARD/.conda-env/riscv-tools/bin" ] && \
  export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found (CHIPYARD=$CHIPYARD)"; exit 1; }

cd "$CP_DIR"
mkdir -p build/logs
echo "=== [1/2] firmware build: $TEST ==="
# The Makefile does not treat EXTRA_CFLAGS changes as a dependency -> always delete and rebuild.
rm -f "build/${TEST}.elf"
make -f Implementation/Makefile.VERIF src_dir=. F=PMP SF=violation \
     TRACK=baremetal CHIPYARD="$CHIPYARD" CONFIG="$CONFIG" EXTRA_CFLAGS=-DPRELOAD

LOG="build/logs/${TEST}.log"

if [ "$BACKEND" = "spike" ]; then
  echo "=== [2/2] spike run ==="
  spike --isa=rv32imac_zicsr_zifencei -m0x80000000:0x10000000 \
        "build/${TEST}.elf" 2>&1 | tee "$LOG"
else
  SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
  [ -x "$SIM" ] || { echo "[!] simulator not built: $SIM"; exit 1; }
  echo "=== [2/2] simulator run (HTIF console -> stdout) ==="
  stdbuf -o0 -e0 "$SIM" +permissive +permissive-off "build/${TEST}.elf" \
    2>&1 | tee "$LOG"
fi

echo "=== check ==="
if grep -q "PMP_VIOLATION_PASS" "$LOG"; then
  echo "PASS: violation traps after PMP lock (mcause 5/7) and sticky lock confirmed"
else
  echo "FAIL: see the PMP_VIOLATION_FAIL line above for the failure mask"
  echo "      bit definitions: verif/ip/S5740/PMP/violation/Inc/PMP_violation.h"
  exit 1
fi
