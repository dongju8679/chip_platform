/* link.vwp.ld - VWP/HPC (xcelium) variant of link.ld
 *
 * Identical to link.ld except for where tohost/fromhost land.
 *
 * Why this file exists
 *   The stock link.ld lets .tohost fall wherever it lands after .bss, which on
 *   a recent build put tohost at 0x80004148. The VWP xcelium harness appears to
 *   watch a fixed address instead of reading the ELF symbol: a working sample
 *   there (mem_test.riscv) has
 *       tohost   = 0x80020000
 *       fromhost = 0x80020040
 *   With our ELF the harness saw nothing, so a CoreMark run executed for 582 s
 *   and exited with no console output at all.
 *
 *   This script pins tohost/fromhost to the addresses that sample uses.
 *
 * Status: NOT CONFIRMED with the hardware team. Two things still need an answer
 *   (see HTIF_ADDRESS_QUESTIONS.md):
 *     1. does the harness read the ELF symbol, or a hardcoded address?
 *     2. is the 0x40 gap between tohost and fromhost required, or incidental?
 *   The gap is reproduced here on the assumption that it matters.
 *
 * Do not use this on dscloud. There the stock link.ld is verified 8/8 and fesvr
 * resolves the symbols from the ELF, so no fixed address is needed.
 *
 * Usage:
 *   make -f Implementation/Makefile.VERIF ... \
 *        LDSCRIPT=Platform/Chipset/TinyRocket/link.vwp.ld
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    /* code/data: 64KB, ends before host mailbox (0x80010000), 4K gap */
    ram  (rwxa) : ORIGIN = 0x80000000, LENGTH = 60K
    /* htif window observed in the VWP sample; outside the code region */
    htif (rw)   : ORIGIN = 0x80020000, LENGTH = 128
}

PROVIDE(__stack_size = 0x1000);   /* 4KB stack */

SECTIONS
{
    . = ORIGIN(ram);

    .text.init : { *(.text.init) } > ram

    .text : {
        *(.text .text.*)
        *(.system_code)          /* rt-dev SYS_FUNCTION */
        *(.sys_init)             /* rt-dev SYS_INIT */
        *(.user_init)            /* rt-dev USER_INIT */
    } > ram

    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > ram

    . = ALIGN(8);
    __global_pointer$ = . + 0x400;

    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
        *(.system_data)          /* rt-dev SYS_DATA */
    } > ram

    . = ALIGN(4);
    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ram

    /* host communication symbols, pinned for the VWP harness.
     * tohost   -> 0x80020000
     * fromhost -> 0x80020040   (matches the mem_test.riscv layout) */
    .tohost 0x80020000 : {
        tohost = .;
        . += 0x40;
        fromhost = .;
        . += 8;
    } > htif

    . = ALIGN(16);
    PROVIDE(__heap_start = .);

    /* stack at top of the 64K code region (below mailbox 0x80010000) */
    PROVIDE(__stack_top = ORIGIN(ram) + LENGTH(ram));
}
