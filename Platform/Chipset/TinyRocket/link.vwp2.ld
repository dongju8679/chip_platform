/* link.vwp2.ld - VWP/HPC variant, matching the mem_test.riscv memory layout
 *
 * Why this exists
 *   link.vwp.ld pinned tohost/fromhost to 0x80020000/0x80020040 and the harness
 *   did start reading our exit code - so HTIF communication works. But the
 *   firmware then exited with CODE_FATAL (2) after ~23 ms of simulated time,
 *   which is the trap handler reporting an unexpected exception during boot.
 *
 *   The working sample on that machine, mem_test.riscv, is laid out as:
 *       LOAD  0x80000000  0x19c8  R E      code
 *       LOAD  0x80020000  0x42cc  RW       data/bss, with tohost at its start
 *       (0x80002000 .. 0x8001FFFF left empty)
 *
 *   Our stock script instead assumes one contiguous 60K region at 0x80000000
 *   and puts the stack at its top, 0x8000F000 - right in the middle of the gap
 *   that mem_test avoids. If that range is not backed by memory on this SoC, the
 *   first stack push faults and the run dies exactly as observed.
 *
 *   This script therefore uses only the two regions mem_test is known to use.
 *
 *       0x80000000  code (text/rodata)          32K window
 *       0x80020000  tohost/fromhost, data, bss, heap, stack   64K window
 *
 * Status: HYPOTHESIS. The actual memory map of this SoC has not been confirmed.
 *   If this works, the gap really is unbacked. If it fails the same way, the
 *   cause is elsewhere and the memory map should be asked for directly.
 *
 * Usage:
 *   make -f Implementation/Makefile.VERIF ... \
 *        LDSCRIPT=Platform/Chipset/TinyRocket/link.vwp2.ld
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    /* code only - matches the first LOAD segment of mem_test.riscv */
    code (rx)  : ORIGIN = 0x80000000, LENGTH = 32K
    /* everything writable - matches the second LOAD segment */
    data (rw)  : ORIGIN = 0x80020000, LENGTH = 64K
}

PROVIDE(__stack_size = 0x1000);   /* 4KB stack */

SECTIONS
{
    /* ---- code region ---------------------------------------------- */
    . = ORIGIN(code);

    .text.init : { *(.text.init) } > code

    .text : {
        *(.text .text.*)
        *(.system_code)          /* rt-dev SYS_FUNCTION */
        *(.sys_init)             /* rt-dev SYS_INIT */
        *(.user_init)            /* rt-dev USER_INIT */
    } > code

    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > code

    /* ---- data region ---------------------------------------------- */
    /* tohost/fromhost first, so they land at 0x80020000 / 0x80020040 exactly
     * as in mem_test.riscv. */
    .tohost ORIGIN(data) : {
        tohost = .;
        . += 0x40;
        fromhost = .;
        . += 8;
    } > data

    . = ALIGN(8);
    __global_pointer$ = . + 0x400;

    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
        *(.system_data)          /* rt-dev SYS_DATA */
    } > data

    . = ALIGN(4);
    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > data

    . = ALIGN(16);
    PROVIDE(__heap_start = .);

    /* stack at the top of the data region, inside memory the sample is known
     * to use - not in the 0x80002000..0x8001FFFF gap */
    PROVIDE(__stack_top = ORIGIN(data) + LENGTH(data));
}
