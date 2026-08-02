#!/usr/bin/env bash
# build_sim.sh - run apply_backend.sh and the Verilator simulator build in one go,
#                in the order that cannot fail (works on a brand-new chipyard tree
#                as well as one that has already been built).
#
# Background: the resource list chipyard copies into gen-collateral does not include
#       verif_host.h / verif_primitives.h. So applying apply_backend.sh directly to a
#       brand-new chipyard (never built) makes the compile fail:
#           gen-collateral/SimTSI.cc:6: fatal error: verif_host.h: No such file or directory
#       (on a tree that already has gen-collateral, apply_backend.sh copies the headers
#        there itself, so there is no problem)
#
# Fix: if gen-collateral does not exist yet -> build once in the pristine state to
#      create it -> then apply apply_backend.sh -> then rebuild incrementally.
#      In this order the failure never happens.
#
# Usage:
#   verif/backend/chipyard/build_sim.sh <CHIPYARD_PATH> [CONFIG]
# Run this from the chip_platform root (it calls apply_backend.sh by relative path).
set -euo pipefail

CY="${1:?usage: build_sim.sh <CHIPYARD_PATH> [CONFIG]}"
CONFIG="${2:-RV32RocketConfig}"
HERE="$(cd "$(dirname "$0")" && pwd)"      # verif/backend/chipyard
GEN="$CY/sims/verilator/generated-src/chipyard.harness.TestHarness.$CONFIG"

if [ ! -d "$GEN/gen-collateral" ]; then
  echo "== no gen-collateral (a brand-new chipyard) -> build once in its pristine state first =="
  ( cd "$CY/sims/verilator" && make -j"$(nproc)" VM_PARALLEL_BUILDS="$(nproc)" CONFIG="$CONFIG" )
  echo "== pristine build complete, gen-collateral created =="
else
  echo "== gen-collateral already present -> skipping the pristine build step =="
fi

echo "== applying apply_backend.sh =="
"$HERE/apply_backend.sh" "$CY" "$CONFIG"

echo "== incremental rebuild (only SimTSI.o is recompiled; fast) =="
( cd "$CY/sims/verilator" && make -j"$(nproc)" VM_PARALLEL_BUILDS="$(nproc)" CONFIG="$CONFIG" )

echo "== build complete =="
echo "run: $CY/sims/verilator/simulator-chipyard.harness-$CONFIG +permissive +verif=<TEST> +max-cycles=<N> +permissive-off <elf>"
