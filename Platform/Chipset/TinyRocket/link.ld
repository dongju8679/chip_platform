/* link.ld - v301 rt-dev port (chipyard DRAM, Phase 1)
 *
 * chipyard RV32RocketConfig: DRAM @ 0x80000000.
 *   - v294 used 16K TCM; rt-dev infra is larger, so use 256K DRAM window.
 *   - host mailbox at 0x80010000 (hal_shim/BREAK_POINT_ADDR) is OUTSIDE this
 *     code region on purpose? NO - must ensure code+data < 0x10000 so it
 *     doesn't collide with mailbox 0x80010000. 256K would overlap 0x80010000!
 *     -> keep code region 0x80000000..0x8000FFFF (64K), mailbox at 0x80010000.
 * Entry _start; provides riscv-tests tohost/fromhost (fesvr/TSI polling).
 * Adds rt-dev section attrs (.system_code/.system_data/.sys_init/.user_init).
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    /* code/data: 64KB, ends before host mailbox (0x80010000), 4K gap */
    ram (rwxa) : ORIGIN = 0x80000000, LENGTH = 60K
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

    /* host communication symbols polled by fesvr/TSI */
    . = ALIGN(8);
    .tohost ALIGN(8) : {
        tohost = .;
        . += 8;
        fromhost = .;
        . += 8;
    } > ram

    . = ALIGN(16);
    PROVIDE(__heap_start = .);

    /* stack at top of the 64K code region (below mailbox 0x80010000) */
    PROVIDE(__stack_top = ORIGIN(ram) + LENGTH(ram));
}
