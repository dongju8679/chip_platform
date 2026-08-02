#!/usr/bin/env bash
# run_regress.sh - fast regression CI on the spike backend (chip_platform v302)
#
# Goal: replace "run each case by hand after every integration" with a single
# invocation that finishes in tens of seconds.
#
# What it does
#   1) Builds the firmware for all v302 features -> build regression
#      (compile/link breakage is detected immediately)
#   2) Runs CA_measure on spike -> PASS + per-block CPI compared against the
#      reference values in ci/ca_baseline.txt
#   3) Prints a summary table plus failure log paths; exits non-zero if any FAIL
#
# Why only CA_measure is actually "verified" on spike (see docs/ci.md for the
# full survey)
#   spike (libriscv) runs the self-timing firmware (CA_measure) unmodified, but
#   the remaining cases need a host Vseq / UART-PLIC-CLINT timer device models /
#   HTIF console handling to be verified - that is the job of RTL (full mode).
#   On the spike side the signal is simply "does it build".
#
# Modes
#   fast (default)  spike only. Tens of seconds. Needs only the RISCV toolchain
#                   plus libriscv.
#   full            fast + RTL. RTL only executes a *pre-built* simulator passed
#                   via --chipyard. (It never builds RTL - safety rule. If the
#                   simulator is missing, the case is SKIPped.)
#
# Usage
#   ci/run_regress.sh                         # fast (default)
#   ci/run_regress.sh fast
#   ci/run_regress.sh full --chipyard /path/to/chipyard-copy
#   ci/run_regress.sh --only CA_measure       # a single case
#   ci/run_regress.sh --update-baseline       # refresh CA CPI reference values
#   RISCV=/path/to/riscv-tools ci/run_regress.sh
#
# exit code: 0 = all PASS, 1 = one or more FAIL, 2 = environment/usage error
set -uo pipefail

# -- paths ---------------------------------------------------------
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP="$(cd "$HERE/.." && pwd)"                 # chip_platform top level
BASELINE="$HERE/ca_baseline.txt"
CONFIG="${CONFIG:-RV32RocketConfig}"
LOGDIR="$CP/build/ci-logs"
SPIKE_TIMEOUT="${SPIKE_TIMEOUT:-120}"        # per-case spike cap (seconds). CA_measure takes a few.

# RISCV supplies both the toolchain (riscv64-unknown-elf-gcc) and libriscv
# (linked by vp/spike).
# The default below is the dscloud reference environment. On any other
# machine set RISCV in the environment or in ~/.scr_env.
[ -f "$HOME/.scr_env" ] && . "$HOME/.scr_env"
# dscloud
RISCV="${RISCV:-$HOME/test_merge_out}"
# VWP / HPC (closed network)
#RISCV="${RISCV:-/user/rocket/users/dongju2/test_merge_out}"

# -- argument parsing ----------------------------------------------
MODE=fast
CHIPYARD="${CHIPYARD:-}"
ONLY=""
UPDATE_BASELINE=0
while [ $# -gt 0 ]; do
  case "$1" in
    fast|full)          MODE="$1";;
    --chipyard)         CHIPYARD="${2:?--chipyard needs a path}"; shift;;
    --chipyard=*)       CHIPYARD="${1#--chipyard=}";;
    --only)             ONLY="${2:?--only needs a test name}"; shift;;
    --only=*)           ONLY="${1#--only=}";;
    --update-baseline)  UPDATE_BASELINE=1;;
    -h|--help)          sed -n '2,36p' "${BASH_SOURCE[0]}"; exit 0;;
    *) echo "[!] unknown argument: $1  (-h for usage)"; exit 2;;
  esac
  shift
done

# -- toolchain setup -----------------------------------------------
if [ ! -x "$RISCV/bin/riscv64-unknown-elf-gcc" ]; then
  echo "[!] riscv toolchain not found: $RISCV/bin/riscv64-unknown-elf-gcc"
  echo "    Set RISCV=<riscv-tools prefix> (libriscv must live under the same prefix)."
  exit 2
fi
export PATH="$RISCV/bin:$PATH"
export LD_LIBRARY_PATH="$RISCV/lib:${LD_LIBRARY_PATH:-}"

mkdir -p "$LOGDIR"

# -- test registry -------------------------------------------------
#   The v302 feature set (CA_measure covers both spike and RTL from one ELF).
#   spike_kind: verify (build+run+check) | build (build only; RTL does the
#   checking) together with the reason.
#   Build flags are kept identical to each case's
#   verif/ip/S5740/<F>/<SF>/run.sh.
#
#   Order is "ascending full-mode RTL runtime" (measured,
#   RV32RocketConfig/verilator):
#       CLINT_sw 70s | EXCEPTION 88s | PMP 103s | CA 363s | dhrystone 547s
#       | coremark 577s | FREERTOS 1049s | UART_printf 4490s (75 min!)
#     Running the short ones first surfaces failures sooner. UART_printf costs
#     43k cycles per character and alone eats 60% of the total, so it must stay
#     last.
#     (In fast mode everything takes seconds, so the order only matters for
#      full.)
TESTS=(CLINT_sw_interrupt EXCEPTION_traps PMP_violation CA_measure \
       BENCH_dhrystone BENCH_coremark FREERTOS_preempt UART_printf)

# Per-case make arguments (built as an array because EXTRA_CFLAGS can contain spaces)
build_args() {  # $1=name  -> fills the global array MK with make arguments
  MK=(-f Implementation/Makefile.VERIF src_dir=. TRACK=baremetal CONFIG="$CONFIG")
  case "$1" in
    CA_measure)         MK+=(F=CA SF=measure OPT=-O2 EXTRA_CFLAGS=-DPRELOAD);;
    CLINT_sw_interrupt) MK+=(F=CLINT SF=sw_interrupt EXTRA_CFLAGS=-DPRELOAD);;
    UART_printf)        MK+=(F=UART SF=printf EXTRA_CFLAGS=-DPRELOAD);;
    FREERTOS_preempt)   MK+=(F=FREERTOS SF=preempt USE_FREERTOS=1 OPT=-O2 EXTRA_CFLAGS=-DPRELOAD);;
    BENCH_coremark)     MK+=(F=BENCH SF=coremark USE_BENCH=coremark BENCH_ITERATIONS=2 OPT=-O2 EXTRA_CFLAGS=-DPRELOAD);;
    BENCH_dhrystone)    MK+=(F=BENCH SF=dhrystone USE_BENCH=dhrystone OPT=-O2 EXTRA_CFLAGS=-DPRELOAD);;
    # The two below are not in the default TESTS (run them via --only).
    # Both are self-check + HTIF console output, so there is no host Vseq.
    EXCEPTION_traps)    MK+=(F=EXCEPTION SF=traps EXTRA_CFLAGS=-DPRELOAD);;
    PMP_violation)      MK+=(F=PMP SF=violation EXTRA_CFLAGS=-DPRELOAD);;
  esac
}

spike_kind()  { case "$1" in CA_measure) echo verify;; *) echo build;; esac; }
spike_note()  { # why spike cannot fully verify this case (build-only cases)
  case "$1" in
    CLINT_sw_interrupt) echo "needs host Vseq(MSIP) - RTL only";;
    UART_printf)        echo "UART0 MMIO not modelled - RTL only";;
    FREERTOS_preempt)   echo "CLINT timer + UART - RTL only";;
    BENCH_coremark)     echo "needs HTIF console handling - RTL only";;
    BENCH_dhrystone)    echo "needs HTIF console handling - RTL only";;
    EXCEPTION_traps)    echo "trap entry / mepc resume - RTL only";;
    PMP_violation)      echo "PMP violation trap (mcause 5/7) - RTL only";;
    *) echo "";;
  esac
}

# -- spike driver setup (build it when missing or stale) -----------
SPIKE_RUN="$CP/vp/spike/verif_spike_run"
if [ ! -x "$SPIKE_RUN" ]; then
  echo "=== building spike driver (vp/spike/build.sh) ==="
  RISCV="$RISCV" "$CP/vp/spike/build.sh" || { echo "[!] spike driver build failed"; exit 2; }
fi

# -- result accumulation -------------------------------------------
declare -A R_BUILD R_SPIKE R_RTL R_TIME R_NOTE
FAILED=0
CA_METRICS=""   # used by --update-baseline

cd "$CP"

# -- firmware build ------------------------------------------------
build_fw() {  # $1=name -> R_BUILD, R_TIME(build)
  local name="$1" log="$LOGDIR/build_${name}.log" t0 t1
  build_args "$name"
  rm -f "build/${name}.elf"
  t0=$(date +%s.%N)
  if make "${MK[@]}" >"$log" 2>&1 && [ -f "build/${name}.elf" ]; then
    R_BUILD[$name]=ok
  else
    R_BUILD[$name]=FAIL; FAILED=1
  fi
  t1=$(date +%s.%N)
  R_TIME[$name]=$(awk "BEGIN{printf \"%.1f\", $t1-$t0}")
}

# -- CA_measure spike verification (PASS + CPI baseline comparison) --
verify_ca() {  # precondition: build/CA_measure.elf exists
  local log="$LOGDIR/spike_CA_measure.log"
  setsid timeout "$SPIKE_TIMEOUT" "$SPIKE_RUN" build/CA_measure.elf +verif=CA_measure </dev/null >"$log" 2>&1
  local rc=$?
  if [ $rc -eq 124 ]; then R_SPIKE[CA_measure]="FAIL(timeout)"; R_NOTE[CA_measure]="spike timeout ${SPIKE_TIMEOUT}s"; FAILED=1; return; fi
  if ! grep -q "CA_measure co-sim PASS" "$log"; then
    R_SPIKE[CA_measure]="FAIL(run)"; R_NOTE[CA_measure]="PASS marker missing -> $log"; FAILED=1; return
  fi

  # Extract measurements (output goes to stderr and is merged into the log)
  #   block table:  [verif_spike] LDUSE  <cyc_raw> <ins_raw> <cyc_net> <ins_net> <CPI> <cyc/iter>
  #   PAIRS      :  [verif_spike] LDUSE  LDINDEP  <dInstr> <dCycle> <cyc/event>  ...
  #   Block-table rows have a numeric $3 (cyc_raw); CONTROLLED PAIRS rows have a
  #   name in $3 (block_B) - that is how the two are told apart.
  local ldu dmi lue cme
  ldu=$(awk '$2=="LDUSE" && $3 ~ /^[0-9]+$/ {print $7}' "$log" | head -1)  # block table CPI column ($7)
  dmi=$(awk '$2=="DMISS" && $3 ~ /^[0-9]+$/ {print $7}' "$log" | head -1)
  lue=$(awk '$2=="LDUSE" && $3=="LDINDEP" {print $6}' "$log" | head -1)    # PAIRS cyc/event ($6)
  cme=$(awk '$2=="DMISS" && $3=="DHIT"    {print $6}' "$log" | head -1)
  CA_METRICS="LDUSE_CPI=$ldu DMISS_CPI=$dmi LOADUSE_EVENT=$lue CACHEMISS_EVENT=$cme"

  # compare against baseline
  local bad="" name exp tol meas
  while read -r name exp tol; do
    [ -z "${name:-}" ] && continue; case "$name" in \#*) continue;; esac
    case "$name" in
      LDUSE_CPI)       meas="$ldu";; DMISS_CPI)       meas="$dmi";;
      LOADUSE_EVENT)   meas="$lue";; CACHEMISS_EVENT) meas="$cme";;
      *) continue;;
    esac
    if [ -z "$meas" ]; then bad="$bad $name(measurement failed)"; continue; fi
    awk "BEGIN{d=$meas-$exp; if(d<0)d=-d; exit !(d<=$tol)}" \
      || bad="$bad $name(measured $meas vs baseline $exp+-$tol)"
  done < "$BASELINE"

  if [ -z "$bad" ]; then
    R_SPIKE[CA_measure]=PASS
    R_NOTE[CA_measure]="CPI OK (LDUSE $ldu, DMISS $dmi)"
  else
    R_SPIKE[CA_measure]="FAIL(cpi)"
    R_NOTE[CA_measure]="baseline deviation:$bad -> $log"
    FAILED=1
  fi
}

# -- full mode: RTL (only runs a pre-built simulator; never builds) --
SIM=""
rtl_available() {
  [ -n "$CHIPYARD" ] || return 1
  SIM="$CHIPYARD/sims/verilator/simulator-chipyard.harness-${CONFIG}"
  [ -x "$SIM" ]
}
run_rtl() {  # $1=name - reuses each case's existing run.sh (no duplication)
  local name="$1" rs t0 t1 rc
  case "$name" in
    CA_measure)         rs="verif/ip/S5740/CA/measure/run.sh";;
    CLINT_sw_interrupt) rs="verif/ip/S5740/CLINT/sw_interrupt/run.sh";;   # host Vseq(MSIP) co-sim - RTL only
    UART_printf)        rs="verif/ip/S5740/UART/printf/run.sh";;
    FREERTOS_preempt)   rs="verif/ip/S5740/FREERTOS/preempt/run.sh";;
    BENCH_coremark)     rs="verif/ip/S5740/BENCH/coremark/run.sh";;
    BENCH_dhrystone)    rs="verif/ip/S5740/BENCH/dhrystone/run.sh";;
    EXCEPTION_traps)    rs="verif/ip/S5740/EXCEPTION/traps/run.sh";;
    PMP_violation)      rs="verif/ip/S5740/PMP/violation/run.sh";;
  esac
  if [ -z "$rs" ] || [ ! -x "$rs" ]; then R_RTL[$name]="SKIP(no-runner)"; return; fi
  t0=$(date +%s.%N)
  CHIPYARD="$CHIPYARD" CONFIG="$CONFIG" MODE=fast "$rs" >"$LOGDIR/rtl_${name}.log" 2>&1
  rc=$?
  t1=$(date +%s.%N)
  R_TIME[$name]="${R_TIME[$name]}+$(awk "BEGIN{printf \"%.0f\", $t1-$t0}")s(rtl)"
  if [ $rc -eq 0 ]; then R_RTL[$name]=PASS; else R_RTL[$name]=FAIL; FAILED=1; fi
}

# -- execution -----------------------------------------------------
echo "============================================================================"
echo " chip_platform regression  (mode=$MODE)   CFG=$CONFIG"
echo " RISCV=$RISCV"
[ "$MODE" = full ] && { rtl_available && echo " RTL  =$SIM" || echo " RTL  = (absent -> SKIP)"; }
echo "============================================================================"

RUNSET=("${TESTS[@]}")
[ -n "$ONLY" ] && RUNSET=("$ONLY")

for name in "${RUNSET[@]}"; do
  printf '  %-20s build...\r' "$name"
  build_fw "$name"
  if [ "${R_BUILD[$name]}" = ok ]; then
    if [ "$(spike_kind "$name")" = verify ]; then
      verify_ca
    else
      R_SPIKE[$name]="n/a"; R_NOTE[$name]="$(spike_note "$name")"
    fi
    if [ "$MODE" = full ]; then rtl_available && run_rtl "$name" || R_RTL[$name]="SKIP(no-sim)"; fi
  else
    R_SPIKE[$name]="-"; R_NOTE[$name]="build failed -> $LOGDIR/build_${name}.log"
    [ "$MODE" = full ] && R_RTL[$name]="-"
  fi
done

# -- --update-baseline ---------------------------------------------
if [ "$UPDATE_BASELINE" = 1 ]; then
  if [ -z "$CA_METRICS" ]; then
    echo "[!] no CA measurements (CA_measure did not run). Cannot update baseline."; exit 2
  fi
  eval "$CA_METRICS"
  tmp="$BASELINE.tmp"; : > "$tmp"
  while IFS= read -r line; do
    case "$line" in
      LDUSE_CPI*)       tol=$(echo "$line"|awk '{print $3}'); printf 'LDUSE_CPI        %-7s %s\n' "$LDUSE_CPI" "$tol";;
      DMISS_CPI*)       tol=$(echo "$line"|awk '{print $3}'); printf 'DMISS_CPI        %-7s %s\n' "$DMISS_CPI" "$tol";;
      LOADUSE_EVENT*)   tol=$(echo "$line"|awk '{print $3}'); printf 'LOADUSE_EVENT    %-7s %s\n' "$LOADUSE_EVENT" "$tol";;
      CACHEMISS_EVENT*) tol=$(echo "$line"|awk '{print $3}'); printf 'CACHEMISS_EVENT  %-7s %s\n' "$CACHEMISS_EVENT" "$tol";;
      *) printf '%s\n' "$line";;
    esac
  done < "$BASELINE" >> "$tmp"
  mv "$tmp" "$BASELINE"
  echo "== baseline updated ($BASELINE):  $CA_METRICS"
  echo "   Review the change with git diff before committing (is the model change intended?)."
  # Updating is a deliberate "accept the current values as the baseline" act,
  # so any preceding CPI deviation is considered resolved.
  if [ "${R_SPIKE[CA_measure]:-}" = "FAIL(cpi)" ]; then
    R_SPIKE[CA_measure]="PASS(updated)"; R_NOTE[CA_measure]="baseline updated: $CA_METRICS"; FAILED=0
  fi
fi

# -- summary table -------------------------------------------------
echo
echo "============================================================================"
if [ "$MODE" = full ]; then
  printf " %-20s %-6s %-14s %-14s %-11s %s\n" TEST BUILD SPIKE RTL TIME NOTE
else
  printf " %-20s %-6s %-14s %-9s %s\n" TEST BUILD SPIKE TIME NOTE
fi
echo "----------------------------------------------------------------------------"
for name in "${RUNSET[@]}"; do
  if [ "$MODE" = full ]; then
    printf " %-20s %-6s %-14s %-14s %-11s %s\n" \
      "$name" "${R_BUILD[$name]}" "${R_SPIKE[$name]:-}" "${R_RTL[$name]:-}" "${R_TIME[$name]:-}s" "${R_NOTE[$name]:-}"
  else
    printf " %-20s %-6s %-14s %-9s %s\n" \
      "$name" "${R_BUILD[$name]}" "${R_SPIKE[$name]:-}" "${R_TIME[$name]:-}s" "${R_NOTE[$name]:-}"
  fi
done
echo "----------------------------------------------------------------------------"

# -- replay failures only ------------------------------------------
if [ "$FAILED" -ne 0 ]; then
  echo " FAIL detail:"
  for name in "${RUNSET[@]}"; do
    b="${R_BUILD[$name]:-}"; s="${R_SPIKE[$name]:-}"; r="${R_RTL[$name]:-}"
    case "$b$s$r" in *FAIL*) echo "   - $name : ${R_NOTE[$name]:-}"
      [ "$b" = FAIL ] && echo "       log: $LOGDIR/build_${name}.log"
      case "$s" in *FAIL*) echo "       log: $LOGDIR/spike_${name}.log";; esac
      case "$r" in *FAIL*) echo "       log: $LOGDIR/rtl_${name}.log";; esac
      ;;
    esac
  done
fi

nbuilt=0; nver=0
for name in "${RUNSET[@]}"; do
  [ "${R_BUILD[$name]}" = ok ] && nbuilt=$((nbuilt+1))
  [ "${R_SPIKE[$name]:-}" = PASS ] && nver=$((nver+1))
done
echo "----------------------------------------------------------------------------"
if [ "$FAILED" -eq 0 ]; then
  echo " RESULT: PASS   ($nbuilt/${#RUNSET[@]} built, $nver spike-verified)"
else
  echo " RESULT: FAIL   (see detail above)"
fi
echo "============================================================================"
exit "$FAILED"
