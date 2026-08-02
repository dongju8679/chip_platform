#!/usr/bin/env bash
# debug.sh - RTL (Verilator) + OpenOCD + GDB debug session entry point (stage 2)
#
#   GDB --:3333--> OpenOCD --rbb(TCP)--> SimJTAG --> DebugTransportModuleJTAG
#                                                    --> DebugModule --> RocketTile
#
# * No-build rule: this script only runs an ALREADY BUILT simulator binary.
#   It never regenerates or rebuilds RTL. It never touches the pristine trees
#
# * What gets loaded into the sim is a "never-ending stub" (vp/rtl/stub/jtag_stub.elf).
#   If real firmware is loaded over TSI, the program writes tohost before OpenOCD has
#   even finished target examine, the sim hits $stop, and the JTAG socket gets a
#   broken pipe. So the stub keeps running, and the firmware under debug is loaded
#   from GDB with `load` through the Debug Module (same flow as flashing a real chip
#   with a debugger). DBG_ELF is used only as "the symbol file to hand to GDB".
#
# usage:
#   vp/rtl/debug.sh [DBG_ELF] [CONFIG]
#   vp/rtl/debug.sh build/debug/CA_measure.elf RV32RocketConfig
#   RUN_ELF=build/debug/foo.elf vp/rtl/debug.sh   # run real firmware over TSI instead of the stub
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CY="${CY:?set CY to a chipyard tree with a built simulator}"
OCD="${OCD:-$ROOT/.tools/openocd-deb}"             # locally extracted OpenOCD

DBG_ELF="${1:-$ROOT/build/debug/CA_measure.elf}"   # symbol file handed to GDB
CONFIG="${2:-RV32RocketConfig}"
STUB="$HERE/stub/jtag_stub.elf"
# What actually runs in the sim: the stub by default (never ends). Override with RUN_ELF.
ELF="${RUN_ELF:-$STUB}"
SIM="$CY/sims/verilator/simulator-chipyard.harness-${CONFIG}"

if [ ! -f "$STUB" ] && [ -z "${RUN_ELF:-}" ]; then
  echo "[rtl] building the stub: $STUB"
  "${RISCV:-$CY/.conda-env/riscv-tools}/bin/riscv64-unknown-elf-gcc" \
      -march=rv32imac_zicsr_zifencei -mabi=ilp32 -nostdlib -nostartfiles \
      -T "$HERE/stub/jtag_stub.ld" -o "$STUB" "$HERE/stub/jtag_stub.S" 2>/dev/null
fi
[ -f "$ELF" ] || { echo "[!] ELF not found: $ELF"; exit 1; }
[ -f "$DBG_ELF" ] || { echo "[!] symbol ELF not found: $DBG_ELF (build it with -g via vp/spike/debug.sh first)"; exit 1; }
[ -x "$SIM" ] || { echo "[!] simulator not found: $SIM"; exit 1; }
[ -x "$OCD/usr/bin/openocd" ] || {
  echo "[!] OpenOCD not found: $OCD"
  echo "    Install: see chip_docs/verif/docs/debugging.md 2.2 (apt-get download + dpkg -x, no sudo needed)"
  exit 1; }

LOGDIR="$ROOT/build/logs"; mkdir -p "$LOGDIR"
RTL_LOG="$LOGDIR/rtl_jtag.log"
OCD_LOG="$LOGDIR/openocd.log"

cleanup() { [ -n "${SIM_PID:-}" ] && kill "$SIM_PID" 2>/dev/null || true
            [ -n "${OCD_PID:-}" ] && kill "$OCD_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

# -- 1) simulator (waits for JTAG remote bitbang) --
echo "[rtl] [1/3] starting the simulator: $(basename "$SIM")  elf=$(basename "$ELF")"
stdbuf -o0 -e0 "$SIM" +permissive +jtag_rbb_enable=1 +permissive-off "$ELF" \
  > "$RTL_LOG" 2>&1 &
SIM_PID=$!

# * The port differs every run. SimJTAG.cc constructs remote_bitbang_t(0), so the OS
#   assigns an arbitrary port and the sim prints it to stderr.
echo -n "[rtl]      waiting for the remote bitbang port"
RBB_PORT=""
for _ in $(seq 1 60); do
  RBB_PORT="$(grep -oP 'Listening on port \K[0-9]+' "$RTL_LOG" 2>/dev/null | head -1 || true)"
  [ -n "$RBB_PORT" ] && break
  echo -n "."; sleep 1
done
echo
[ -n "$RBB_PORT" ] || { echo "[!] port not found - check $RTL_LOG"; exit 1; }
echo "[rtl]      RBB_PORT=$RBB_PORT"

# -- 2) OpenOCD --
echo "[rtl] [2/3] starting OpenOCD (rocket_rbb.cfg)"
export RBB_PORT
export LD_LIBRARY_PATH="$OCD/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
stdbuf -o0 -e0 "$OCD/usr/bin/openocd" \
  -s "$OCD/usr/share/openocd/scripts" \
  -f "$HERE/rocket_rbb.cfg" > "$OCD_LOG" 2>&1 &
OCD_PID=$!

# -- 3) wait + instructions --
#   * Every JTAG bit costs a TCP round trip plus Verilator cycles, so examine takes minutes.
echo "[rtl] [3/3] waiting for target examine (JTAG is slow - expect several minutes)"
for i in $(seq 1 180); do
  if grep -qE 'Listening on port 3333 for gdb' "$OCD_LOG" 2>/dev/null; then
    cat <<EOF

[rtl] Ready.  (loaded into the simulator: $(basename "$ELF"))
  -- from another terminal --
  export PATH=$CY/.conda-env/riscv-tools/bin:\$PATH
  cd $ROOT
  riscv64-unknown-elf-gdb $DBG_ELF \\
      -ex 'set remotetimeout 600' \\
      -ex 'target extended-remote :3333' \\
      -ex 'monitor halt' \\
      -ex 'load' \\
      -ex 'set \$pc = _start' \\
      -ex 'break main' -ex 'continue'

  NOTE: the simulator is running an infinite-loop stub. The 'load' above puts
     $DBG_ELF into memory through the Debug Module (the same flow as flashing
     with a debugger on real silicon).
     load goes over JTAG and is slow - expect several minutes for tens of KB.

  Logs:  $OCD_LOG
         $RTL_LOG
  To quit: Ctrl-C in this terminal (the simulator and OpenOCD shut down together)

EOF
    wait $OCD_PID
    exit 0
  fi
  if grep -qE '^Error:' "$OCD_LOG" 2>/dev/null; then
    echo "[!] OpenOCD error:"; grep -E '^Error:' "$OCD_LOG" | head; exit 1
  fi
  kill -0 $OCD_PID 2>/dev/null || { echo "[!] OpenOCD terminated"; tail -20 "$OCD_LOG"; exit 1; }
  sleep 5
  [ $((i % 12)) -eq 0 ] && echo "[rtl]      ... $((i*5))s  ($(tail -1 "$OCD_LOG"))"
done
echo "[!] timeout (15 min). Check $OCD_LOG"
exit 1
