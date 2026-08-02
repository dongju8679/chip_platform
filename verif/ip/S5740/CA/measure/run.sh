#!/usr/bin/env bash
# run.sh - build and run CA_measure (chipyard backend, execution-driven CA measurement)
#
# Same pattern as the UART_printf/FREERTOS run.sh. Differences:
#   - +verif=CA_measure is passed -> verif_tsi_t reads the result table and
#     prints it as a table.
#   - -DPRELOAD is required (so bp_init() in crt.S does not wait for the host)
#   - The UART costs tens of seconds per character, so it is OFF by default.
#     Enable it with CA_UART=1 (very slow).
#
# usage: ./verif/ip/S5740/CA/measure/run.sh
#        CHIPYARD=/path CONFIG=RV32RocketConfig CA_UART=1 ./...
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"   # chip_platform top level

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"
CA_UART="${CA_UART:-0}"
TEST=CA_measure

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

EXTRA="-DPRELOAD"
[ "$CA_UART" = "1" ] && EXTRA="$EXTRA -DCA_UART"

cd "$CP_DIR"
echo "=== [1/2] firmware build: $TEST (OPT=-O2, $EXTRA) ==="
# The Makefile does not treat EXTRA_CFLAGS changes as a dependency (docs/FreeRTOS_port.md 8.2).
#   Toggling CA_UART between runs would otherwise reuse the ELF built with the previous flags.
rm -f "build/${TEST}.elf"
make -f Implementation/Makefile.VERIF src_dir=. F=CA SF=measure \
     TRACK=baremetal CHIPYARD="$CHIPYARD" CONFIG="$CONFIG" OPT=-O2 EXTRA_CFLAGS="$EXTRA"

SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
[ -x "$SIM" ] || { echo "[!] simulator not built: $SIM"; exit 1; }

echo "=== [2/2] simulator run: +verif=$TEST ==="
mkdir -p build/logs
stdbuf -o0 -e0 "$SIM" +permissive +verif="$TEST" +permissive-off \
  "build/${TEST}.elf" 2>&1 | tee "build/logs/${TEST}.log"

echo "=== check ==="
if grep -q "CA_measure co-sim PASS" "build/logs/${TEST}.log"; then
  echo "PASS: CA cycle measurement complete (see the table in the log above)"
else
  echo "FAIL: CA result table not found"; exit 1
fi
