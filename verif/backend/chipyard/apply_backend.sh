#!/usr/bin/env bash
# apply_backend.sh - apply the chipyard backend into a chipyard tree (one command, auto version detect)
#
# Injects the framework chipyard adapter (verif_host.h + SimTSI include method) into an arbitrary
# chipyard path. Auto-detects the chipyard version (1.10 / 1.13) and uses the matching SimTSI include.
#
# Usage:
#   verif/backend/chipyard/apply_backend.sh <CHIPYARD_PATH> [CONFIG]
# Example:
#   verif/backend/chipyard/apply_backend.sh ~/chipyard_v110 RV32RocketConfig   # 1.10
#   verif/backend/chipyard/apply_backend.sh ~/chipyard      RV32RocketConfig   # 1.13
#
# -- include method + auto version detect ------------------------------
# chipyard SimTSI.cc differs in structure by version:
#   1.10: single tsi  (testchip_tsi_t *tsi = NULL;)            -> SimTSI.cc.include.v110
#   1.13: chip_id map (std::map<int, testchip_tsi_t*> tsis;) -> SimTSI.cc.include.v113
# Both are "stock SimTSI.cc + #include verif_host.h + (create verif_tsi_t on +verif=)".
# This script detects the version by whether the target SimTSI.cc has 'std::map', and
# selects the matching include. The verif_tsi_t impl (PLIC/CLINT co-sim) is in the common verif_host.h.
#
# Memory-map addresses like UART are auto-extracted from dts by the Makefile at build time (not touched here).
# Config uses the chipyard default RV32RocketConfig (no separate injection).
set -euo pipefail

CY="${1:?usage: apply_backend.sh <CHIPYARD_PATH> [CONFIG]}"
CONFIG="${2:-RV32RocketConfig}"
HERE="$(cd "$(dirname "$0")" && pwd)"          # verif/backend/chipyard
ROOT="$(cd "$HERE/../../.." && pwd)"              # chip_platform root

CSRC="$CY/generators/testchipip/src/main/resources/testchipip/csrc"
GEN="$CY/sims/verilator/generated-src/chipyard.harness.TestHarness.$CONFIG"

[ -d "$CY" ]    || { echo "[!] chipyard not found: $CY"; exit 2; }
[ -d "$CSRC" ]  || { echo "[!] testchipip csrc not found: $CSRC"; exit 2; }
[ -f "$HERE/verif_host.h" ] || { echo "[!] verif_host.h not found: $HERE"; exit 2; }

# -- version detect -------------------------------------------------
# If the target SimTSI.cc (or .orig backup) has std::map -> 1.13, else 1.10.
DETECT_SRC="$CSRC/SimTSI.cc"
[ -f "$CSRC/SimTSI.cc.orig" ] && DETECT_SRC="$CSRC/SimTSI.cc.orig"   # if already applied, detect from stock
if grep -q "std::map<int, testchip_tsi_t" "$DETECT_SRC" 2>/dev/null; then
  VER="v113"
elif grep -q "testchip_tsi_t \*tsi" "$DETECT_SRC" 2>/dev/null; then
  VER="v110"
else
  # if current SimTSI.cc is already our include and undetectable, re-detect by map
  if grep -q "tsis\[chip_id\]" "$CSRC/SimTSI.cc" 2>/dev/null; then VER="v113"; else VER="v110"; fi
fi
INCLUDE_SRC="$HERE/SimTSI.cc.include.$VER"
[ -f "$INCLUDE_SRC" ] || { echo "[!] $INCLUDE_SRC not found (detected version=$VER)"; exit 2; }
echo "== version detected: $VER (using SimTSI.cc.include.$VER) =="

echo "== 1) copy adapter header + seam (csrc) =="
cp "$HERE/verif_host.h" "$CSRC/"
cp "$ROOT/verif/include/verif_primitives.h" "$CSRC/"
echo "  -> $CSRC/{verif_host.h, verif_primitives.h}"

# -- rt-dev Vseq support --
# verif_host.h #includes the Vseq .cc, which in turn includes S5740_mcu_tests.h and the
# test's own Inc/*.h. SimTSI.cc is compiled with -I<gen-collateral> only, so every one of
# those files has to sit next to it. Copying the test Inc/*.h is also what makes the host
# and the DUT share one address map instead of two copies that can drift apart.
cp "$HERE/S5740_mcu_tests.h"      "$CSRC/"
cp "$HERE/verif_addr_chipyard.h"  "$CSRC/"
cp "$HERE/verif_vseq_reset.h"     "$CSRC/"
echo "  -> $CSRC/S5740_mcu_tests.h (rt-dev host API as free functions)"
echo "  -> $CSRC/{verif_addr_chipyard.h, verif_vseq_reset.h} (DUT address map, per-test macro reset)"
# verif/framework holds the framework's own demo sequences (DEMO_handshake); same layout as verif/ip.
VSEQ_N=0
for f in $(find "$ROOT/verif/ip" "$ROOT/verif/framework" -path '*/Vseq/*.cc' 2>/dev/null); do
  cp "$f" "$CSRC/"; VSEQ_N=$((VSEQ_N+1))
done
for f in $(find "$ROOT/verif/ip" "$ROOT/verif/framework" -path '*/Inc/*.h' 2>/dev/null); do
  cp "$f" "$CSRC/"
done
echo "  -> $CSRC/ : $VSEQ_N Vseq .cc + test Inc/*.h"

# -- FAKE STUB (home build-test only) --
# verif_tsi_t::trigger_irq() calls sim_ext_intr_trigger(). The real implementation is
# testchipip's SimExtInterrupts.cc, which is only compiled when a HarnessBinder
# (WithSimExtInterrupts) pulls it in. So whether it exists depends on CONFIG:
#   RV32RocketExtConfig : SimExtInterrupts.cc is in gen-collateral -> real IRQ injection
#   RV32RocketConfig    : absent -> link fails, undefined reference `sim_ext_intr_trigger'
#
# * The test must NOT be "does the header exist". A chipyard csrc tree may carry the
#   header (declaration only) regardless of CONFIG, so testing the header would wrongly
#   conclude "the real one is present" even on a CONFIG that has no implementation, skip
#   the stub, and break the link. (That reproduces the moment you run this script a
#   second time: the fake header in gen-collateral gets overwritten by the real
#   declaration.) So the test is "is the implementation actually part of this build".
USE_FAKE_EXTINTR=0
if [ -f "$HERE/testchip_external_interrupts.h" ]; then
  if [ -d "$GEN/gen-collateral" ]; then
    # Already-built tree -> gen-collateral IS the list of files that actually compile.
    [ -f "$GEN/gen-collateral/SimExtInterrupts.cc" ] || USE_FAKE_EXTINTR=1
  else
    # First build, no evidence to judge from -> keep the old behaviour
    # (stub only when the header is missing).
    [ -f "$CSRC/testchip_external_interrupts.h" ] || USE_FAKE_EXTINTR=1
  fi
fi
if [ "$USE_FAKE_EXTINTR" = 1 ]; then
  # Back up the real header once, so it can be restored.
  # NOTE: if this script already installed the fake stub on an earlier run, the .orig
  #       backup below captures the STUB, not the pristine header. On a tree whose
  #       testchipip never had this header (e.g. vanilla chipyard 1.13.0), there is
  #       nothing to restore - delete both files before switching to a CONFIG that
  #       has the real SimExtInterrupts.cc.
  [ -f "$CSRC/testchip_external_interrupts.h" ] && [ ! -f "$CSRC/testchip_external_interrupts.h.orig" ] \
    && cp "$CSRC/testchip_external_interrupts.h" "$CSRC/testchip_external_interrupts.h.orig"
  cp "$HERE/testchip_external_interrupts.h" "$CSRC/"
  echo "  -> $CSRC/testchip_external_interrupts.h (FAKE STUB - SimExtInterrupts.cc is absent from this CONFIG build)"
  echo "     trigger_irq() only logs; it does not drive the RTL lines."
  echo "     For real IRQ injection, build with CONFIG=RV32RocketExtConfig (WithSimExtInterrupts)."
else
  # * csrc is shared across CONFIGs. If an earlier run installed the fake stub for a
  #   different CONFIG (e.g. RV32RocketConfig), it MUST be reverted when coming back to
  #   a CONFIG that has the real implementation. Otherwise the stub's static inline
  #   definition collides with the real definition in SimExtInterrupts.cc.
  if [ -f "$CSRC/testchip_external_interrupts.h.orig" ] \
     && grep -q "FAKE" "$CSRC/testchip_external_interrupts.h" 2>/dev/null; then
    cp "$CSRC/testchip_external_interrupts.h.orig" "$CSRC/testchip_external_interrupts.h"
    echo "  [=] a fake stub was installed -> restored the real header (.orig)."
  fi
  echo "  [=] SimExtInterrupts.cc is part of the build -> using the real implementation; no fake stub installed."
fi

echo "== 2) replace SimTSI.cc with include version ($VER) =="
# stock backup (first time). If current SimTSI.cc is already an include (references verif_host.h), skip backup.
if [ ! -f "$CSRC/SimTSI.cc.orig" ]; then
  if grep -q 'verif_host.h' "$CSRC/SimTSI.cc"; then
    echo "  [!] current SimTSI.cc is already an include version - not stock. Skipping .orig backup."
  else
    cp "$CSRC/SimTSI.cc" "$CSRC/SimTSI.cc.orig"
    echo "  backing up stock SimTSI.cc -> $CSRC/SimTSI.cc.orig"
  fi
fi
cp "$INCLUDE_SRC" "$CSRC/SimTSI.cc"
echo "  -> $CSRC/SimTSI.cc (include $VER)"

echo "== 3) reflect into gen-collateral (if already built) =="
if [ -d "$GEN/gen-collateral" ]; then
  cp "$CSRC/SimTSI.cc"          "$GEN/gen-collateral/"
  cp "$CSRC/verif_host.h"       "$GEN/gen-collateral/"
  cp "$CSRC/verif_primitives.h" "$GEN/gen-collateral/"
  cp "$CSRC/S5740_mcu_tests.h"     "$GEN/gen-collateral/"
  cp "$CSRC/verif_addr_chipyard.h" "$GEN/gen-collateral/"
  cp "$CSRC/verif_vseq_reset.h"    "$GEN/gen-collateral/"
  for f in $(find "$ROOT/verif/ip" "$ROOT/verif/framework" \( -path '*/Vseq/*.cc' -o -path '*/Inc/*.h' \) 2>/dev/null); do
    cp "$f" "$GEN/gen-collateral/"
  done
  [ -f "$CSRC/testchip_external_interrupts.h" ] && cp "$CSRC/testchip_external_interrupts.h" "$GEN/gen-collateral/"
  rm -f "$GEN/chipyard.harness.TestHarness.$CONFIG/SimTSI.o"
  rm -f "$CY/sims/verilator/simulator-chipyard.harness-$CONFIG"
  echo "  overwrote gen-collateral (+ Vseq/.cc, test Inc/.h) + deleted SimTSI.o/binary"
else
  echo "  no gen-collateral = first build -> csrc replacement is applied automatically"
fi

echo
echo "== done ($VER). Next: =="
echo "  cd $CY/sims/verilator && make CONFIG=$CONFIG"
echo "  run: ./simulator-chipyard.harness-$CONFIG +permissive +verif=<TEST> +max-cycles=1000000 +permissive-off <elf>"
