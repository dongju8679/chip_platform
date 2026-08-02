/* irq_latency.ld - a minimal linker script (DRAM 0x80000000, segments without headers) */
OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS
{
    . = 0x80000000;
    .text : { *(.text*) }
    . = ALIGN(8);
    .data : { *(.data*) *(.sdata*) }
    .bss  : { *(.bss*)  *(.sbss*)  }
    /DISCARD/ : { *(.riscv.attributes) *(.comment) }
}
