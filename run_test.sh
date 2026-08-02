#!/usr/bin/env bash
# run_test.sh - build + run a test (chipyard backend)
#   usage: ./run_test.sh CLINT rtc
#         ./run_test.sh PLIC latency
#
#   required directory layout (put chip_platform and chipyard under the same parent):
#     <root>/
#     |-- chipyard/        (chipyard 1.13 baseline, 1.10 auto-compatible)
#     +-- chip_platform/   (this repo - this script is at its top level)
#
#   Paths are auto-computed from this script location, so no extra env setup is needed.
set -euo pipefail

# -- auto path computation -----------------------------------------
# This script (run_test.sh) is at the chip_platform top level.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHIP_PLATFORM_DIR="$SCRIPT_DIR"
CHIPYARD_DIR="$(dirname "$CHIP_PLATFORM_DIR")/chipyard"

# -- layout check --------------------------------------------------
# Verify that chipyard is really at the agreed location (sibling folder).
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

# -- load chipyard environment -------------------------------------
# The riscv compiler (riscv64-unknown-elf-gcc) lives in the chipyard conda environment.
# Try in 3 steps so it works regardless of conda init.
if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  echo "=== load chipyard environment ==="
  # 1) initialize conda in the shell (so env.sh conda activate works)
  CONDA_BASE="$(conda info --base 2>/dev/null || true)"
  if [ -n "$CONDA_BASE" ] && [ -f "$CONDA_BASE/etc/profile.d/conda.sh" ]; then
    source "$CONDA_BASE/etc/profile.d/conda.sh"
  fi
  # 2) load the proper environment via chipyard env.sh (succeeds if conda is initialized)
  source "$CHIPYARD_DIR/env.sh" 2>/dev/null || true
  # 3) if the compiler is still missing, bypass conda - add toolchain path to PATH directly
  if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
    export PATH="$CHIPYARD_DIR/.conda-env/riscv-tools/bin:$PATH"
  fi
fi
# final check: if still missing, print a clear message and exit
command -v riscv64-unknown-elf-gcc >/dev/null 2>&1 || {
  echo "[!] riscv64-unknown-elf-gcc not found."
  echo "    check the chipyard toolchain (conda) installation: $CHIPYARD_DIR"
  exit 1
}

# -- arguments -----------------------------------------------------
F="${1:?usage: run_test.sh <F> <SF>   e.g.: run_test.sh CLINT rtc}"
SF="${2:?usage: run_test.sh <F> <SF>}"
CONFIG="${CONFIG:-RV32RocketConfig}"

# -- build + run ---------------------------------------------------
cd "$CHIP_PLATFORM_DIR"
TEST="${F}_${SF}"
echo "=== [1/2] firmware build: $TEST ==="
# -DPRELOAD is required on the chipyard backend, co-sim cases included.
#   verif/backend/chipyard/verif_host.h does an unconditional `#define PRELOAD 1`
#   right before it includes the Vseq files, because fesvr(testchip_tsi_t) loads the
#   ELF into DRAM itself -- that load *is* the preload. So the host never issues
#   host_set_bp(BP_MEM_WRITE) for any test.
#   If the firmware is then built without -DPRELOAD, crt.S -> bp_init() waits in
#   dut_wait_bp(BP_MEM_WRITE) for a write that never comes. Today it still limps
#   along only because dut_wait_bp() happens to also accept BP_TEST_begin, which the
#   host sets later in its loop -- i.e. it races bp_init() instead of handshaking.
#   Keep host and DUT on the same protocol: always -DPRELOAD here.
#   (chip_docs/verif/docs/verif_host_refactor.md section 5)
make -f Implementation/Makefile.VERIF src_dir=. F="$F" SF="$SF" \
     CHIPYARD="$CHIPYARD_DIR" CONFIG="$CONFIG" EXTRA_CFLAGS=-DPRELOAD

ELF="build/${TEST}.elf"
[ -f "$ELF" ] || { echo "[!] ELF not found: $ELF"; exit 1; }

echo "=== [2/2] simulator run: +verif=$TEST ==="
"$CHIPYARD_DIR/sims/verilator/simulator-chipyard.harness-${CONFIG}" \
   +permissive +verif="$TEST" +permissive-off "$ELF"
