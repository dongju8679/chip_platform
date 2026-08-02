#!/usr/bin/env bash
# build.sh - build the spike backend
#   offline (porting / CI): without SPIKE_NATIVE -> compile check only (no sim produced)
#   real runs: SPIKE_NATIVE + link against libriscv
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
RISCV="${RISCV:-}"

INCS="-I$ROOT/verif/include -I$HERE"

if [ -n "$RISCV" ] && [ -d "$RISCV/include/riscv" ]; then
  echo "[spike] SPIKE_NATIVE build (libriscv: $RISCV)"
  FLAGS="-DSPIKE_NATIVE -I$RISCV/include"
  LIBS="-L$RISCV/lib -lriscv -lfesvr -Wl,-rpath,$RISCV/lib"
  # older installs ship disasm as a separate library (link only when present)
  if [ -e "$RISCV/lib/libriscv_disasm.so" ] || [ -e "$RISCV/lib/libriscv_disasm.a" ]; then
    LIBS="$LIBS -lriscv_disasm"
  fi
  g++ -std=c++17 -O2 $FLAGS $INCS "$HERE/verif_spike_run.cc" $LIBS -o "$HERE/verif_spike_run"
  g++ -std=c++17 -O2 $FLAGS $INCS "$HERE/verif_ca_run.cc"   $LIBS -o "$HERE/verif_ca_run"
  # DRAM mailbox variant (for chipyard-track ELFs: BP/TEST addresses 0x8001000x).
  #   thanks to the #ifndef guards in verif_primitives.h this is swapped with -D alone.
  g++ -std=c++17 -O2 $FLAGS $INCS \
      -DVERIF_BREAK_ADDR=0x80010000u -DVERIF_TEST_ADDR=0x80010004u \
      "$HERE/verif_ca_run.cc" $LIBS -o "$HERE/verif_ca_run_dram"
  # GDB debug driver (gdb_rsp.h - RSP server). Separate executable from the normal driver.
  g++ -std=c++17 -O2 $FLAGS $INCS "$HERE/verif_spike_gdb.cc" $LIBS -o "$HERE/verif_spike_gdb"
  echo "[spike] built: verif_spike_run, verif_ca_run, verif_ca_run_dram, verif_spike_gdb"
else
  echo "[spike] offline compile check (RISCV unset -> no sim is constructed)"
  g++ -std=c++17 -fsyntax-only $INCS "$HERE/verif_spike_run.cc" && echo "  OK verif_spike_run.cc"
  g++ -std=c++17 -fsyntax-only $INCS "$HERE/verif_ca_run.cc"   && echo "  OK verif_ca_run.cc"
  g++ -std=c++17 -fsyntax-only $INCS "$HERE/verif_spike_gdb.cc" && echo "  OK verif_spike_gdb.cc"
  echo "[spike] to actually run, set RISCV=<libriscv prefix> and rebuild"
fi
