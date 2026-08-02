#!/usr/bin/env bash
# run.sh - run verification with the rt-dev backend
#   usage: backend/rt-dev/run.sh <ELF> TEST=<F>_<SF>
#   Loads the ELF into the rt-dev original engine (company Verilator flow) and drives it via verif_rtdev_t.
set -euo pipefail
ELF="${1:?usage: run.sh <ELF> TEST=<name>}"; shift || true
TEST=""; for a in "$@"; do case "$a" in TEST=*) TEST="${a#TEST=}";; esac; done

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

# rt-dev original engine path (company environment; set via env var)
RTDEV="${RTDEV:-/mnt/sdc/_project/rt_dev}"
[ -d "$RTDEV" ] || { echo "[!] rt-dev engine not found: $RTDEV (set RTDEV=path)"; exit 2; }

echo "[rt-dev] run TEST=$TEST ELF=$ELF"
# Invoke the rt-dev original simulator (wire to the company flow).
#   The original takes +verif=<TEST>-style args via the import_test.py dispatcher.
"$RTDEV/sim/run" +verif="$TEST" "$ELF" || {
  echo "[rt-dev] (skeleton) the actual rt-dev engine call must be wired to the company environment"; exit 1;
}
