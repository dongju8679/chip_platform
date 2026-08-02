#!/usr/bin/env bash
#
# riscv-dv random instruction verification for RV32RocketConfig.
#
#   generate (pyflow) -> gcc -> spike (ISS) -> Rocket RTL (Verilator) -> compare
#
# A mismatch between the two traces means the RTL and the ISA reference
# disagree about architectural state, i.e. an RTL bug candidate.
#
# Usage:
#   ./run_riscv_dv.sh [-t TEST] [-n ITERATIONS] [-c INSTR_CNT] [-s SEED]
#
set -euo pipefail

# ---------------------------------------------------------------- environment
CY="${CY:?set CY to a chipyard tree with a built simulator}"
DV="${DV:?set DV to the riscv-dv checkout}"
VENV="${VENV:?set VENV to the riscv-dv virtualenv}"
WORK="${WORK:-$PWD/riscv-dv-work}"

export RISCV=$CY/.conda-env/riscv-tools
export RISCV_GCC=$RISCV/bin/riscv64-unknown-elf-gcc
export RISCV_OBJCOPY=$RISCV/bin/riscv64-unknown-elf-objcopy
export SPIKE_PATH=$RISCV/bin

TARGET=rv32imac
CFG=RV32RocketConfig
SIM=$CY/sims/verilator/simulator-chipyard.harness-$CFG

TEST=riscv_arithmetic_basic_test
ITER=1
SEED=""
# Stack size in words. riscv-dv's default; kept adjustable because the stack
# is emitted as explicit zeros, so it inflates the image. That only matters
# without +loadmem (see the RTL step below) -- with it, loading is a memcpy.
STACK_LEN=5000
# log2 of the trap-vector alignment. 7 => 128 B, which is what Rocket requires
# in vectored mtvec mode.
TVEC_ALIGN=7

while getopts "t:n:s:k:a:h" opt; do
  case $opt in
    t) TEST=$OPTARG ;;
    n) ITER=$OPTARG ;;
    s) SEED=$OPTARG ;;
    k) STACK_LEN=$OPTARG ;;
    a) TVEC_ALIGN=$OPTARG ;;
    h) sed -n '2,12p' "$0"; exit 0 ;;
    *) exit 1 ;;
  esac
done

# --- safety: never build or run inside the pristine chipyard checkouts -------
case "$CY" in
  */chipyard-riscv-dv) ;;
  *) echo "REFUSING: CY=$CY is not the dedicated chipyard-riscv-dv copy"; exit 1 ;;
esac
[[ -x $SIM ]] || { echo "Missing RTL simulator: $SIM"; exit 1; }

source $VENV/bin/activate

OUT=$WORK/$TEST
RTL=$OUT/rtl_sim
mkdir -p "$RTL"

echo "=============================================================="
echo " riscv-dv  target=$TARGET  cfg=$CFG"
echo " test=$TEST  iterations=$ITER"
echo " output=$OUT"
echo "=============================================================="

# ------------------------------------------------- 1. generate + compile + ISS
GEN_ARGS=(--target "$TARGET" --test "$TEST" --simulator pyflow --iss spike
          --steps gen,gcc_compile,iss_sim -o "$OUT" --iterations "$ITER"
          # "=" form: the value itself starts with "--", so a space-separated
          # argument would be parsed as another option.
          #
          # Note: --tvec_alignment is passed for the record, but it does not
          # actually decide the alignment -- tvec_alignment is a randomised
          # vsc variable whose soft constraint pins it to cfg.tvec_ceil. The
          # real fix is in riscv-dv's tvec_ceil (see README); this just keeps
          # the intent visible in the command line.
          "--sim_opts=--stack_len=$STACK_LEN --tvec_alignment=$TVEC_ALIGN")
[[ -n $SEED ]] && GEN_ARGS+=(--seed "$SEED")

echo ">>> [1/3] generate + gcc + spike"
( cd "$DV" && python3 run.py "${GEN_ARGS[@]}" )

# ---------------------------------------------------- 2. RTL sim + 3. compare
overall=0
for ((i = 0; i < ITER; i++)); do
  elf=$OUT/asm_test/${TEST}_$i.o
  [[ -f $elf ]] || { echo "!! missing ELF $elf"; overall=1; continue; }

  echo ">>> [2/3] RTL (Verilator) : ${TEST}_$i"
  # +permissive brackets the plusargs so HTIF does not try to open them as the
  # target binary; +verbose turns on the trace printf.
  # +loadmem is the important one: it loads the ELF straight into the backing
  # memory through DPI instead of pushing it over the serial TSI link. Without
  # it fesvr spends millions of simulated cycles transferring the image before
  # it releases the core from the boot ROM, and the run looks hung.
  ( cd "$CY/sims/verilator" && \
    ./simulator-chipyard.harness-$CFG +permissive +verbose \
      "+loadmem=$elf" +permissive-off "$elf" ) \
    > "$RTL/${TEST}_$i.raw.log" 2>&1 || true

  # Commit-log lines carry no disassembly, so tag them with DASM(<bits>) and
  # let spike-dasm expand it. +verbose lines already end in DASM(...) and are
  # left alone (they start with "C0:", which this pattern does not match).
  sed -E 's/^([0-9] 0x[0-9a-f]+ \(0x([0-9a-f]+)\).*)$/\1 DASM(\2)/' \
    "$RTL/${TEST}_$i.raw.log" \
    | "$RISCV/bin/spike-dasm" > "$RTL/${TEST}_$i.log"

  # The program ends with ecall, which traps. Spike's converter stops at that
  # ecall, so stop the RTL trace where the ecall lands -- otherwise the trap
  # handler and the tohost write show up as spurious trailing mismatches.
  end_pc=$("$RISCV/bin/riscv64-unknown-elf-nm" "$elf" \
           | awk '$3 == "mtvec_handler" { print "0x"$1 }')

  echo ">>> [3/3] trace compare : ${TEST}_$i"
  python3 "$DV/scripts/spike_log_to_trace_csv.py" \
    --log "$OUT/spike_sim/${TEST}_$i.log" --csv "$RTL/spike_$i.csv" > /dev/null 2>&1
  python3 "$DV/scripts/rocket_log_to_trace_csv.py" \
    --log "$RTL/${TEST}_$i.log" --csv "$RTL/rocket_$i.csv" \
    ${end_pc:+--end_pc "$end_pc"} > /dev/null 2>&1

  # instr_trace_compare opens its log with "a+", so clear it first or a re-run
  # appends to the previous verdict.
  rm -f "$RTL/compare_$i.log"
  python3 "$DV/scripts/instr_trace_compare.py" \
    --csv_file_1 "$RTL/spike_$i.csv"  --csv_name_1 spike \
    --csv_file_2 "$RTL/rocket_$i.csv" --csv_name_2 "rocket_rtl" \
    --log "$RTL/compare_$i.log" > /dev/null 2>&1 || true

  result=$(grep -o '\[PASSED\].*\|\[FAILED\].*' "$RTL/compare_$i.log" | tail -1)
  echo "    ${TEST}_$i : ${result:-NO RESULT}"
  [[ $result == \[PASSED\]* ]] || overall=1
done

echo "=============================================================="
[[ $overall -eq 0 ]] && echo " OVERALL: PASSED" || echo " OVERALL: FAILED"
echo " logs: $RTL"
echo "=============================================================="
exit $overall
