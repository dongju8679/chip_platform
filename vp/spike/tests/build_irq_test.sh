#!/usr/bin/env bash
# build_irq_test.sh - build irq_latency.elf (rv32imac, linked at DRAM 0x80000000)
#   requires: riscv64-unknown-elf-gcc (from chipyard env.sh or $RISCV/bin)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
riscv64-unknown-elf-gcc -march=rv32imac_zicsr -mabi=ilp32 \
  -nostdlib -nostartfiles -T "$HERE/irq_latency.ld" \
  "$HERE/irq_latency.S" -o "$HERE/irq_latency.elf"
echo "built: $HERE/irq_latency.elf"
