#!/usr/bin/env bash
# run.sh - build and run the FreeRTOS 2-task preemption demo (chipyard backend)
#
# Same conventions as UART_printf/run.sh:
#   -DPRELOAD : there is no host (Vseq), so the bp_init() mailbox handshake in
#               crt.S is skipped
#   +verif=   : not passed -> standard testchip_tsi_t
# Additionally:
#   USE_FREERTOS=1 : adds Middleware/FreeRTOS to the build (requires
#                    TRACK=baremetal)
#
# usage:
#   ./verif/ip/S5740/FREERTOS/preempt/run.sh              # spec build (1 kHz tick)
#   MODE=fast ./verif/ip/S5740/FREERTOS/preempt/run.sh    # quick functional check (50 kHz tick)
#
# Note on runtime
#   verilator measures at about 2,700 cycles/s.
#     1 kHz tick     = 500,000 cycles = about 3 min (wall) per tick
#     1 UART char    = 43,400 cycles  = about 16 s
#   The default (10 switches) takes roughly 30-50 minutes. MODE=fast finishes in
#   a few minutes.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"
MODE="${MODE:-spec}"
TEST=FREERTOS_preempt

if [ "$MODE" = "fast" ]; then
  # Raise the tick to 50 kHz, cutting CPU cycles per tick to 1/50 (for a port functional check).
  RTOS_FLAGS="-DFREERTOS_TICK_RATE_HZ=50000 -DPREEMPT_SWITCH_TARGET=6"
  TAG=fast
else
  # Required specification: 1 kHz tick, observe 10 preemptive switches
  RTOS_FLAGS=""
  TAG=spec
fi

if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  source "$CHIPYARD/env.sh" 2>/dev/null || true
  command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || \
    export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"
fi
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found (CHIPYARD=$CHIPYARD)"; exit 1; }

cd "$CP_DIR"
echo "=== [1/2] firmware build: $TEST ($TAG) ==="
# The Makefile does not treat EXTRA_CFLAGS changes as a dependency. Running
#   with different MODE values would reuse the ELF built with the previous flags
#   (this actually bit us once: an ELF with a fake spike UART address baked in
#   nearly made it onto RTL).
#   So we always delete and rebuild - it is only 12 files and takes seconds.
rm -f "build/${TEST}.elf"
# OPT=-O2 is pinned: -Os turns 64-bit shifts into libgcc calls
#              (__ashldi3/__lshrdi3), and this toolchain has no rv32 libgcc
#              (same root cause as docs/UART_printf.md 3.3).
make -f Implementation/Makefile.VERIF src_dir=. F=FREERTOS SF=preempt \
     TRACK=baremetal USE_FREERTOS=1 OPT=-O2 \
     CHIPYARD="$CHIPYARD" CONFIG="$CONFIG" \
     EXTRA_CFLAGS="-DPRELOAD $RTOS_FLAGS"

SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
[ -x "$SIM" ] || { echo "[!] simulator not built: $SIM"; exit 1; }

echo "=== [2/2] simulator run (UART0 -> stdout) ==="
mkdir -p build/logs
LOG="build/logs/${TEST}_${TAG}.log"
stdbuf -o0 -e0 "$SIM" +permissive +permissive-off "build/${TEST}.elf" 2>&1 | tee "$LOG"

echo "=== check ==="
# Preemption is proven only when all 3 criteria hold.
#   (1) the RTOSOK marker is printed -> the scheduler reached the target switch count
#   (2) both c1 and c2 != 0          -> both tasks really executed
#   (3) '1' and '2' alternate in the output -> switches were forced on every tick
if ! grep -q "RTOSOK" "$LOG"; then
  echo "FAIL: RTOSOK marker missing (scheduler never started, or no ticks fired)"; exit 1
fi
# p=1 is the verdict the firmware computed itself
# (c1!=0 && c2!=0 && switches>=target).
# The same value is carried in the exit code, so the verdict also works on
# backends without a UART.
if ! grep -q "p=1" "$LOG"; then
  echo "FAIL: verdict not satisfied - one task never ran, or there were too few switches"
  grep -oE "RTOSOK.*" "$LOG" || true
  exit 1
fi
# Switch sequence: extract only the 1s and 2s between 'R' and the RTOSOK report.
# They must strictly alternate.
#   Do NOT run tr -cd '12' over the whole log - it would pick up 1s and 2s from
#   numbers such as c1=1185 c2=1165 on the RTOSOK line and mix in switches that
#   never happened.
SEQ=$(sed -n 's/.*\bR\([12]*\).*/\1/p' "$LOG" | head -1)
echo "PASS: FreeRTOS preemptive scheduling confirmed"
echo "  switch seq : $SEQ   (each character = one context switch forced by a tick)"
grep -oE "RTOSOK.*" "$LOG"
