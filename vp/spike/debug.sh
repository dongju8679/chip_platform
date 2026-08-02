#!/usr/bin/env bash
# debug.sh - entry point for a spike + GDB debug session (stage 1)
#
# Does three things:
#   1) rebuild the firmware with debug info (-g) into build/debug/
#      (leaves the existing build/*.elf alone - verification artifacts are preserved)
#   2) make sure verif_spike_gdb (the RSP server) is built
#   3) start the server halted and print the GDB command to attach with
#
# usage:
#   vp/spike/debug.sh <F> <SF> [PORT] [-- extra make vars...]
# examples:
#   vp/spike/debug.sh CA measure                 # build/debug/CA_measure.elf, :3333
#   vp/spike/debug.sh UART printf 3333 -- EXTRA_CFLAGS=-DPRELOAD
#   ELF=build/debug/foo.elf vp/spike/debug.sh    # debug an ELF that already exists
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
cd "$ROOT"

# -- toolchain -- (conda env of a $CY copy; never the pristine ~/chipyard tree)
RISCV="${RISCV:?set RISCV to the combined prefix}"
export RISCV
export PATH="$RISCV/bin:$PATH"
command -v riscv64-unknown-elf-gcc >/dev/null || {
  echo "[!] riscv toolchain not found (RISCV=$RISCV)"; exit 1; }

PORT="${3:-3333}"

# -- 1) firmware (-g) --
if [ -n "${ELF:-}" ]; then
  echo "[debug] using existing ELF: $ELF"
else
  F="${1:?usage: debug.sh <F> <SF> [PORT] [-- make vars...]}"
  SF="${2:?usage: debug.sh <F> <SF> [PORT] [-- make vars...]}"
  shift 3 2>/dev/null || shift $# ; [ "${1:-}" = "--" ] && shift || true
  ELF="build/debug/${F}_${SF}.elf"
  mkdir -p build/debug
  echo "[debug] [1/3] firmware build (-g): $ELF"
  # * Only -g is added to OPT. -O2 is kept, so code generation is identical to the
  #   normal build and only DWARF is attached.
  #   (if stepping jumps around too much, pass OPT='-O0 -g3')
  make -f Implementation/Makefile.VERIF src_dir=. F="$F" SF="$SF" \
       TRACK="${TRACK:-baremetal}" build_dir=build/debug OPT="${OPT:--O2 -g}" "$@"
fi
[ -f "$ELF" ] || { echo "[!] ELF not found: $ELF"; exit 1; }

# -- 2) build the RSP server --
echo "[debug] [2/3] checking verif_spike_gdb"
[ -x "$HERE/verif_spike_gdb" ] || "$HERE/build.sh"

# -- 3) print instructions and run --
cat <<EOF
[debug] [3/3] starting the GDB-wait server (port $PORT)

  -- from another terminal --
  export PATH=$RISCV/bin:\$PATH
  cd $ROOT
  riscv64-unknown-elf-gdb $ELF \\
      -ex 'target remote :$PORT' \\
      -ex 'break main' -ex 'continue'

  Common commands: bt / info registers / next / step / stepi / finish
                   x/16xw \$sp / p/x \$mcause / monitor cycles
  To quit: detach or quit from GDB (the server shuts down with it)

EOF
exec "$HERE/verif_spike_gdb" "$ELF" "+gdb=$PORT" ${GDB_EXTRA:-}
