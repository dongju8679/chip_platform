#!/usr/bin/env bash
#
# Re-stage the untracked testchipip verification sources into gen-collateral.
#
# This copy's testchipip/csrc/SimTSI.cc is locally modified to support "+verif="
# and pulls in verif_host.h and friends. Those files live in csrc but are not
# registered Chisel resources, so the build never copies them into
# gen-collateral/ and the C++ compile dies with:
#
#   gen-collateral/SimTSI.cc:6:10: fatal error: verif_host.h: No such file or directory
#
# Run this after any Verilator rebuild that regenerates gen-collateral.
# Nothing here is riscv-dv specific -- it just makes the tree compile.
#
set -euo pipefail

CY="${CY:?set CY to a chipyard tree with a built simulator}"
CFG=${1:-RV32RocketConfig}

SRC=$CY/generators/testchipip/src/main/resources/testchipip/csrc
DST=$CY/sims/verilator/generated-src/chipyard.harness.TestHarness.$CFG/gen-collateral

# The stub build of this header. The real csrc header only declares the
# SimExtInterrupts DPI functions, and their implementation (SimExtInterrupts.cc)
# is not in the build's file list -- so using it gives undefined references.
# The stub is self-contained and is what this machine has always built with.
STUB="${STUB:?set STUB to a reference testchip_external_interrupts.h}"

[[ -d $SRC ]] || { echo "no testchipip csrc at $SRC"; exit 1; }
[[ -d $DST ]] || { echo "no gen-collateral at $DST (build it first)"; exit 1; }

need=(verif_host.h verif_primitives.h testchip_external_interrupts.h
      sw.h timer.h CA_measure.h UART_printf.h FREERTOS_preempt.h
      S5740_mcu_tests.h
      CLINT_rtc.cc CLINT_rtc.h
      CLINT_sw_interrupt.cc CLINT_sw_interrupt.h
      CLINT_timer_interrupt.cc CLINT_timer_interrupt.h
      I2SR_int_muldiv.cc I2SR_int_muldiv.h
      PLIC_enable.cc PLIC_enable.h
      PLIC_latency.cc PLIC_latency.h
      WFI_hostAccess.cc WFI_hostAccess.h)

n=0
for f in "${need[@]}"; do
  if [[ -f $SRC/$f ]]; then
    cp "$SRC/$f" "$DST/$f"; n=$((n + 1))
  else
    echo "warning: $f not found in csrc"
  fi
done

if [[ -f $STUB ]]; then
  cp "$STUB" "$DST/testchip_external_interrupts.h"
  echo "used stub testchip_external_interrupts.h"
else
  echo "warning: stub header missing, link may fail on sim_ext_intr_* symbols"
fi

echo "staged $n file(s) into $DST"
