#!/usr/bin/env bash
# setup_chipyard.sh - one-time: apply backend + build chipyard simulator
#   usage: ./setup_chipyard.sh
#   note: chipyard build takes tens of minutes. Only first time or on adapter/CONFIG change.
#
#   required directory layout (put chip_platform and chipyard under the same parent):
#     <root>/
#     |-- chipyard/        (chipyard 1.13 baseline, 1.10 auto-compatible)
#     +-- chip_platform/   (this repo - this script is at its top level)
#
#   Paths are auto-computed from this script location, so no extra env setup is needed.
#   The chipyard version (1.10/1.13) is auto-detected by apply_backend.sh.
set -euo pipefail

# -- auto path computation -----------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHIP_PLATFORM_DIR="$SCRIPT_DIR"
CHIPYARD_DIR="$(dirname "$CHIP_PLATFORM_DIR")/chipyard"

# -- layout check --------------------------------------------------
if [ ! -f "$CHIPYARD_DIR/env.sh" ] || [ ! -d "$CHIPYARD_DIR/sims/verilator" ]; then
  echo "[!] chipyard not found: $CHIPYARD_DIR"
  echo ""
  echo "    required directory layout:"
  echo "      <root>/"
  echo "      |-- chipyard/        (chipyard 1.13)"
  echo "      +-- chip_platform/   (this repo)"
  echo ""
  echo "    put chipyard in the same parent folder as chip_platform."
  exit 1
fi

CONFIG="${CONFIG:-RV32RocketConfig}"

# -- load chipyard environment -------------------------------------
# The riscv compiler and build tools live in the chipyard conda environment.
# Try in 3 steps so it works regardless of conda init.
if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  echo "=== load chipyard environment ==="
  # 1) initialize conda in the shell
  CONDA_BASE="$(conda info --base 2>/dev/null || true)"
  if [ -n "$CONDA_BASE" ] && [ -f "$CONDA_BASE/etc/profile.d/conda.sh" ]; then
    source "$CONDA_BASE/etc/profile.d/conda.sh"
  fi
  # 2) load the proper environment via chipyard env.sh
  source "$CHIPYARD_DIR/env.sh" 2>/dev/null || true
  # 3) if the compiler is still missing, bypass conda - add toolchain path to PATH directly
  if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
    export PATH="$CHIPYARD_DIR/.conda-env/riscv-tools/bin:$PATH"
  fi
fi

# -- apply backend + build simulator -------------------------------
cd "$CHIP_PLATFORM_DIR"
echo "=== [1/2] apply backend (inject adapter into chipyard, auto version detect) ==="
verif/backend/chipyard/apply_backend.sh "$CHIPYARD_DIR" "$CONFIG"

echo "=== [2/2] build chipyard simulator (tens of minutes) ==="
cd "$CHIPYARD_DIR/sims/verilator" && make CONFIG="$CONFIG"
echo "=== done. Now test with ./run_test.sh <F> <SF> ==="
