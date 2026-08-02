/* link.vwp3.ld - VWP/HPC (xcelium, SCRConfig) linker script
 *
 * ITIM holds instructions only. Everything the core loads as *data* - including
 * .rodata - must live in DTIM.
 *
 * Memory map, from SCTestHarness.SCRConfig.dts:
 *     itim@80000000  reg = <0x80000000 0x1e000   0x8001e000 0x2000>
 *                    ("mem" 120K, "control" 8K)
 *     dtim@80020000  reg = <0x80020000 0x20000>   (128K)
 *
 * How this differs from link.vwp2.ld
 *   vwp2 put .rodata in ITIM alongside .text, which looked reasonable - both are
 *   read-only. It is not: GCC compiles a switch into a jump table placed in
 *   .rodata, and reads it with a plain load:
 *
 *       addi a5,a5,-2008     # 0x80003828 <errpat+0x10>   table base
 *       slli a0,a0,0x2
 *       add  a0,a0,a5
 *       lw   a5,0(a0)        <-- data load from ITIM -> access fault
 *       jr   a5
 *
 *   The fault was observed in CoreMark's get_seed_32 at cycle 2794, trapping to
 *   _trap_vector_entry and ending in exit(CODE_FATAL). Moving .rodata into DTIM
 *   fixes it.
 *
 *   This is also why mem_test.riscv keeps only 6.5K of code at 0x80000000 and
 *   puts all 17K of its remaining sections at 0x80020000.
 *
 * Usage:
 *   make -f Implementation/Makefile.VERIF ... \
 *        LDSCRIPT=Platform/Chipset/TinyRocket/link.vwp3.ld
 *
 * link.ld is unchanged; dscloud is unaffected.
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    /* ITIM - instruction fetch only */
    code (rx)  : ORIGIN = 0x80000000, LENGTH = 64K
    /* DTIM - everything the core loads or stores */
    data (rw)  : ORIGIN = 0x80020000, LENGTH = 128K
}

PROVIDE(__stack_size = 0x1000);   /* 4KB stack */

SECTIONS
{
    /* ---- ITIM: code only ------------------------------------------ */
    . = ORIGIN(code);

    .text.init : { *(.text.init) } > code

    .text : {
        *(.text .text.*)
        *(.system_code)          /* rt-dev SYS_FUNCTION */
        *(.sys_init)             /* rt-dev SYS_INIT */
        *(.user_init)            /* rt-dev USER_INIT */
    } > code

    /* ---- DTIM: everything else ------------------------------------ */
    /* tohost/fromhost first, so they sit at 0x80020000 / 0x80020040,
     * matching mem_test.riscv. */
    .tohost ORIGIN(data) : {
        tohost = .;
        . += 0x40;
        fromhost = .;
        . += 8;
    } > data

    /* .rodata belongs here, not with .text: jump tables and constant
     * pools are read with ordinary loads. */
    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
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

    /* stack at the top of DTIM */
    PROVIDE(__stack_top = ORIGIN(data) + LENGTH(data));
}
