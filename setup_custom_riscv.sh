#!/usr/bin/env bash
# setup_custom_riscv.sh - build a combined prefix that wires a custom spike plus a
# custom rv32 gcc into the fast regression
#
# Why it is needed (two toolchains, with different roles)
#   RISCV            = the spike engine. Used by ci/run_regress.sh and vp/spike/build.sh.
#                      - vp/spike/build.sh : -I$RISCV/include  -L$RISCV/lib -lriscv -lfesvr
#                                            (plus -lriscv_disasm only when
#                                             libriscv_disasm.{so,a} is present)
#                                            -> the $RISCV/include/riscv directory must
#                                            exist for a SPIKE_NATIVE build
#                      - run_regress.sh safety guard : $RISCV/bin/riscv64-unknown-elf-gcc
#                                            must be executable to pass (fast mode never
#                                            actually runs it, but its presence is required).
#                      - run_regress.sh : PATH=$RISCV/bin:...  LD_LIBRARY_PATH=$RISCV/lib:...
#   RISCV_TOOLCHAIN  = the rv32 firmware compiler. Implementation/Makefile.VERIF picks up
#                      gcc/objdump/nm/size via
#                      RISCV_PREFIX=$(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-.
#
# What this script does
#   It gathers a custom spike install (libriscv/libfesvr/include) plus a riscv64 gcc for
#   the guard into one prefix, "using symlinks only", so it can be used as RISCV.
#   The originals (conda riscv-tools and the like) are only ever read.
#   RISCV_TOOLCHAIN needs no combining, so it is merely validated and exported as is.
#
# Usage
#   ./setup_custom_riscv.sh                                   # combine using the default paths
#   ./setup_custom_riscv.sh --spike <install> --toolchain <rv32prefix> --out <prefix>
#   ./setup_custom_riscv.sh --rebuild-vp        # force-rebuild the vp/spike drivers against the new RISCV
#   eval "$(./setup_custom_riscv.sh --quiet --print-env)"      # receive only the exports
#
# exit code: 0 success / 2 environment or usage error
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# -- machine-local overrides ---------------------------------------
# Paths differ per machine, so they are not hardcoded here beyond the
# reference-environment defaults below. Create ~/.scr_env on any other
# machine and it wins over those defaults:
#
#   : "${SPIKE_INSTALL:=$HOME/opt/riscv/spike/current}"
#   : "${RISCV_TOOLCHAIN:=$HOME/opt/riscv/toolchain/rv32-multilib}"
#   : "${OUT_PREFIX:=$HOME/opt/riscv/merged/custom-verif}"
#   export SPIKE_INSTALL RISCV_TOOLCHAIN OUT_PREFIX GCC64_PREFIX
#
# Precedence: command-line argument > environment > ~/.scr_env > default.
[ -f "$HOME/.scr_env" ] && . "$HOME/.scr_env"

# -- defaults ------------------------------------------------------
# Active block: dscloud. To move to VWP/HPC, comment the dscloud lines and
# uncomment the VWP ones (or just create ~/.scr_env, which wins over both).

# dscloud
SPIKE="${SPIKE_INSTALL:-/share/opt/spike}"
TOOLCHAIN="${RISCV_TOOLCHAIN:-/share/opt/toolchain/gcc13.2_fastinterrupt}"
OUT="${OUT_PREFIX:-$HOME/test_merge_out}"
# The source that supplies the safety guard (riscv64-unknown-elf-gcc). Only read/symlinked.
GCC64="${GCC64_PREFIX:-/tools/chipyard/.conda-env/riscv-tools}"

# VWP / HPC (closed network)
#SPIKE="${SPIKE_INSTALL:-/user/rocket/opt/spike}"
#TOOLCHAIN="${RISCV_TOOLCHAIN:-/user/rocket/opt/toolchain/gcc13.2_fastinterrupt}"
#OUT="${OUT_PREFIX:-/user/rocket/users/dongju2/test_merge_out}"
#GCC64="${GCC64_PREFIX:-/user/rocket/chipyard/.conda-env/riscv-tools}"
REBUILD_VP=0
VP_ROOT="$HERE"
FORCE=0
QUIET=0
PRINT_ENV=0

die() { echo "[!] $*" >&2; exit 2; }
say() { [ "$QUIET" = 1 ] || echo "$@"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --spike)        SPIKE="${2:?--spike needs a path}"; shift;;
    --spike=*)      SPIKE="${1#--spike=}";;
    --toolchain)    TOOLCHAIN="${2:?--toolchain needs a path}"; shift;;
    --toolchain=*)  TOOLCHAIN="${1#--toolchain=}";;
    --out)          OUT="${2:?--out needs a path}"; shift;;
    --out=*)        OUT="${1#--out=}";;
    --gcc64)        GCC64="${2:?--gcc64 needs a path}"; shift;;
    --gcc64=*)      GCC64="${1#--gcc64=}";;
    --rebuild-vp)   REBUILD_VP=1
                    case "${2:-}" in -*|"") ;; *) VP_ROOT="$2"; shift;; esac;;
    --rebuild-vp=*) REBUILD_VP=1; VP_ROOT="${1#--rebuild-vp=}";;
    --force)        FORCE=1;;
    --quiet)        QUIET=1;;
    --print-env)    PRINT_ENV=1;;
    -h|--help)      sed -n '2,32p' "${BASH_SOURCE[0]}"; exit 0;;
    *) die "unknown argument: $1  (-h for usage)";;
  esac
  shift
done

abs() { readlink -f "$1"; }

# -- input validation ----------------------------------------------
[ -d "$SPIKE" ]     || die "custom spike install not found: $SPIKE"
[ -d "$TOOLCHAIN" ] || die "custom rv32 toolchain not found: $TOOLCHAIN"
[ -d "$GCC64" ]     || die "riscv64 gcc prefix not found: $GCC64  (set it with --gcc64)"

# What vp/spike/build.sh actually looks for
[ -d "$SPIKE/include/riscv" ] || die "$SPIKE/include/riscv not found - check that spike was installed with --prefix"
[ -d "$SPIKE/include/fesvr" ] || die "$SPIKE/include/fesvr not found"
ls "$SPIKE"/lib/libriscv.* >/dev/null 2>&1 || die "$SPIKE/lib/libriscv.{so,a} not found"
ls "$SPIKE"/lib/libfesvr.* >/dev/null 2>&1 || die "$SPIKE/lib/libfesvr.{so,a} not found"
# What Makefile.VERIF actually looks for
[ -x "$TOOLCHAIN/bin/riscv32-unknown-elf-gcc" ] || die "$TOOLCHAIN/bin/riscv32-unknown-elf-gcc not found"
# What the run_regress.sh safety guard requires
[ -x "$GCC64/bin/riscv64-unknown-elf-gcc" ] || die "$GCC64/bin/riscv64-unknown-elf-gcc not found (needed for the safety guard)"

# -- output prefix validation: never overwrite an original ---------
for src in "$SPIKE" "$TOOLCHAIN" "$GCC64"; do
  [ "$(abs "$OUT")" = "$(abs "$src")" ] && die "--out equals the original ($src). Damaging originals is forbidden."
  case "$(abs "$OUT")/" in
    "$(abs "$src")"/*) die "--out is inside the original ($src). Damaging originals is forbidden.";;
  esac
done
MARK="$OUT/.custom_riscv_prefix"
if [ -e "$OUT" ]; then
  if [ -f "$MARK" ] || [ "$FORCE" = 1 ]; then
    rm -rf "$OUT"                       # only remove what this script created (or what --force approved)
  else
    die "$OUT already exists and was not created by this script. Overwrite it with --force, or change --out."
  fi
fi

# -- combining: symlinks only (nothing is copied or modified) ------
mkdir -p "$OUT/bin" "$OUT/lib" "$OUT/include"

link_into() {  # $1=srcdir  $2=dstdir  - symlink the entries of srcdir into dstdir
  local s d n
  s="$(abs "$1")"; d="$2"
  [ -d "$s" ] || return 0
  for n in "$s"/*; do
    [ -e "$n" ] || continue
    ln -sfn "$n" "$d/$(basename "$n")"
  done
}

# 1) spike engine: include/{riscv,fesvr,fdt,softfloat}, lib/libriscv*, libfesvr*, libsoftfloat*, libdisasm*, bin/spike*
link_into "$SPIKE/include" "$OUT/include"
link_into "$SPIKE/lib"     "$OUT/lib"
link_into "$SPIKE/bin"     "$OUT/bin"

# 2) riscv64-unknown-elf-* to satisfy the safety guard (fast mode never runs them; only their presence is required)
n64=0
for f in "$GCC64"/bin/riscv64-unknown-elf-*; do
  [ -e "$f" ] || continue
  ln -sfn "$(abs "$f")" "$OUT/bin/$(basename "$f")"
  n64=$((n64+1))
done
[ "$n64" -gt 0 ] || die "failed to symlink riscv64-unknown-elf-*"

# 3) Convenience: put the rv32 tools on PATH too (Makefile.VERIF uses absolute paths, so this changes nothing)
for f in "$TOOLCHAIN"/bin/riscv32-unknown-elf-*; do
  [ -e "$f" ] || continue
  ln -sfn "$(abs "$f")" "$OUT/bin/$(basename "$f")"
done

cat >"$MARK" <<EOF
# Combined prefix created by setup_custom_riscv.sh (symlinks only)
spike=$(abs "$SPIKE")
toolchain=$(abs "$TOOLCHAIN")
gcc64=$(abs "$GCC64")
EOF

# -- self-check: re-verify the run_regress.sh / vp/spike/build.sh requirements --
[ -x "$OUT/bin/riscv64-unknown-elf-gcc" ] || die "guard check failed: $OUT/bin/riscv64-unknown-elf-gcc"
[ -d "$OUT/include/riscv" ]               || die "build check failed: $OUT/include/riscv"
ls "$OUT"/lib/libriscv.* >/dev/null 2>&1  || die "build check failed: $OUT/lib/libriscv.*"

say "[setup] combined prefix created: $OUT"
say "        spike     : $(abs "$SPIKE")   ($("$SPIKE/bin/spike" --help 2>&1 | head -1))"
say "        rv32 gcc  : $(abs "$TOOLCHAIN")  ($("$TOOLCHAIN/bin/riscv32-unknown-elf-gcc" -dumpversion))"
say "        guard gcc64: $(abs "$GCC64")/bin/riscv64-unknown-elf-gcc (symlink, never executed)"

# -- rebuild the vp/spike drivers (mandatory once RISCV has changed) ---
#   run_regress.sh does not rebuild verif_spike_run if it already exists.
#   An existing binary still carries the RUNPATH of the old RISCV, so it must be
#   deleted and rebuilt.
if [ "$REBUILD_VP" = 1 ]; then
  [ -x "$VP_ROOT/vp/spike/build.sh" ] || die "vp/spike/build.sh not found: $VP_ROOT"
  say "[setup] rebuilding the vp/spike drivers ($VP_ROOT/vp/spike)"
  rm -f "$VP_ROOT"/vp/spike/verif_spike_run \
        "$VP_ROOT"/vp/spike/verif_ca_run \
        "$VP_ROOT"/vp/spike/verif_ca_run_dram \
        "$VP_ROOT"/vp/spike/verif_spike_gdb
  vplog="$(mktemp -t vpbuild.XXXXXX.log)"
  if ! RISCV="$OUT" "$VP_ROOT/vp/spike/build.sh" >"$vplog" 2>&1; then
    cat "$vplog" >&2; die "vp/spike rebuild failed (log: $vplog)"
  fi
  rm -f "$vplog"
  say "        RUNPATH: $(readelf -d "$VP_ROOT/vp/spike/verif_spike_run" | sed -n 's/.*RUNPATH.*\[\(.*\)\]/\1/p')"
fi

if [ "$PRINT_ENV" = 1 ]; then
  echo "export RISCV=$OUT"
  echo "export RISCV_TOOLCHAIN=$(abs "$TOOLCHAIN")"
else
  say ""
  say "Use it like this:"
  say "  RISCV=$OUT \\"
  say "  RISCV_TOOLCHAIN=$(abs "$TOOLCHAIN") \\"
  say "  ci/run_regress.sh fast"
fi
