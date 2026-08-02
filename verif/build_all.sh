#!/usr/bin/env bash
# build_all.sh - build every firmware ELF and record a fingerprint (sha256).
#
# Why this exists (regression cost):
#   RTL (verilator) runs are expensive - measured on RV32RocketConfig:
#   UART_printf ~75 min, FREERTOS_preempt ~17 min, BENCH_coremark ~10 min.
#   Re-running all of them after every refactor is not practical.
#   Instead, showing "the ELF is byte-identical before and after the refactor"
#   mechanically guarantees the run result is unchanged. So: take a fingerprint
#   before the refactor, compare after. Only the cases whose fingerprint moved
#   need to be re-run on RTL.
#
# usage:
#   ./verif/build_all.sh snapshot <name>   # build + create build/fingerprint-<name>.txt
#   ./verif/build_all.sh compare <a> <b>   # compare two fingerprints
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CP_DIR"

CHIPYARD="${CHIPYARD:?set CHIPYARD to a chipyard tree with a built simulator}"
CONFIG="${CONFIG:-RV32RocketConfig}"

[ -d "$CHIPYARD/.conda-env/riscv-tools/bin" ] && \
  export PATH="$CHIPYARD/.conda-env/riscv-tools/bin:$PATH"

MK="make -s -f Implementation/Makefile.VERIF src_dir=. CHIPYARD=$CHIPYARD CONFIG=$CONFIG"

build_one() { # build_one <TEST> <make args...>
  local test=$1; shift
  rm -f "build/${test}.elf"
  if $MK "$@" >/dev/null 2>&1; then
    echo "OK"
  else
    echo "BUILD-FAIL"
  fi
}

# All catalogued cases. The flags must match what each case's run.sh uses,
# otherwise the fingerprint compares something the RTL run never executes.
snapshot() {
  local out="build/fingerprint-${1}.txt"
  mkdir -p build
  : > "$out"
  local -a CASES=(
    "CLINT_sw_interrupt|F=CLINT SF=sw_interrupt TRACK=baremetal"
    "CLINT_timer_interrupt|F=CLINT SF=timer_interrupt TRACK=baremetal"
    "UART_printf|F=UART SF=printf TRACK=baremetal EXTRA_CFLAGS=-DPRELOAD"
    "CA_measure|F=CA SF=measure TRACK=baremetal OPT=-O2 EXTRA_CFLAGS=-DPRELOAD"
    "FREERTOS_preempt|F=FREERTOS SF=preempt TRACK=baremetal USE_FREERTOS=1 OPT=-O2 EXTRA_CFLAGS=-DPRELOAD"
    "BENCH_coremark|F=BENCH SF=coremark TRACK=baremetal USE_BENCH=coremark BENCH_ITERATIONS=2 OPT=-O2 EXTRA_CFLAGS=-DPRELOAD"
    "BENCH_dhrystone|F=BENCH SF=dhrystone TRACK=baremetal USE_BENCH=dhrystone OPT=-O2 EXTRA_CFLAGS=-DPRELOAD"
    "PLIC_enable|F=PLIC SF=enable TRACK=baremetal"
    "I2SR_int_muldiv|F=I2SR SF=int_muldiv TRACK=baremetal"
  )
  for c in "${CASES[@]}"; do
    local test="${c%%|*}" args="${c#*|}"
    printf '%-24s ' "$test"
    # shellcheck disable=SC2086
    local st; st=$(build_one "$test" $args)
    if [ "$st" = "OK" ] && [ -f "build/${test}.elf" ]; then
      # * Do NOT hash the whole ELF file.
      #   When gcc compiles and links several .c files in one invocation, the
      #   temporary object names (/tmp/ccXXXXXX.o) end up in the symbol table, so
      #   building the same source twice produces different ELF bytes (confirmed).
      #   Hash only what actually lands on the chip:
      #     - objcopy -O binary : the loaded section bytes as-is
      #     - nm -n (file symbols excluded) : symbol layout
      #   If those two match, what the DUT executes is identical.
      local h bin; bin=$(mktemp)
      riscv64-unknown-elf-objcopy -O binary "build/${test}.elf" "$bin"
      h=$( { cat "$bin";
             riscv64-unknown-elf-nm -n "build/${test}.elf" | grep -v ' a \| N ' ; } \
           | sha256sum | cut -c1-16 )
      rm -f "$bin"
      local sz; sz=$(riscv64-unknown-elf-size "build/${test}.elf" | tail -1 | awk '{print $1"/"$2"/"$3}')
      echo "$h  text/data/bss=$sz"
      echo "$test $h $sz" >> "$out"
    else
      echo "$st"
      echo "$test BUILD-FAIL -" >> "$out"
    fi
  done
  echo "--> $out"
}

compare() {
  local a="build/fingerprint-${1}.txt" b="build/fingerprint-${2}.txt"
  [ -f "$a" ] && [ -f "$b" ] || { echo "[!] fingerprint file not found"; exit 1; }
  local diff_n=0
  while read -r test h sz; do
    local hb; hb=$(awk -v t="$test" '$1==t{print $2}' "$b")
    local szb; szb=$(awk -v t="$test" '$1==t{print $3}' "$b")
    if [ "$h" = "$hb" ]; then
      printf '  SAME     %-24s %s\n' "$test" "$h"
    else
      printf '  CHANGED  %-24s %s -> %s   (%s -> %s)\n' "$test" "$h" "${hb:-none}" "$sz" "${szb:-none}"
      diff_n=$((diff_n+1))
    fi
  done < "$a"
  echo "changed: $diff_n"
}

case "${1:-}" in
  snapshot) snapshot "${2:?name}";;
  compare)  compare "${2:?a}" "${3:?b}";;
  *) echo "usage: $0 snapshot <name> | compare <a> <b>"; exit 2;;
esac
