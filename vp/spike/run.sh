#!/usr/bin/env bash
# run.sh - spike backend run entry point (invoked by `make run BACKEND=spike` in Makefile.VERIF)
#
# args: <ELF_PATH> [CONFIG=...] [TEST=...]
# directories used:
#   - Implementation/build/<TEST>.elf   (the firmware passed in - built by Makefile.VERIF)
#   - vp/spike/verif_spike_run     (produced by build.sh)
#   - dist/<TARGET>-spike-<CONFIG>/     (result record - created here)
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

ELF="${1:?usage: run.sh <ELF> [CONFIG=..] [TEST=..]}"; shift || true
TEST=""
for kv in "$@"; do
  case "$kv" in
    TEST=*) TEST="${kv#TEST=}";;
  esac
done

# build the driver (first time, or when it changed)
[ -x "$HERE/verif_spike_run" ] || "$HERE/build.sh"

# run: verif_spike_run <elf> +verif=<TEST>
"$HERE/verif_spike_run" "$ELF" ${TEST:+"+verif=$TEST"}
