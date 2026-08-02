/* riscv-dv linker script adapted for Chipyard RV32RocketConfig.
 *
 * Same layout as riscv-dv's scripts/link.ld (DRAM at 0x80000000, tohost/
 * fromhost picked up by fesvr), with one change: the generator emits
 * .user_stack and .kernel_stack as 20 KB of explicit zeros each. In the
 * stock script those land in a PROGBITS segment, so fesvr has to push
 * 40 KB of zeros over the serial HTIF link before it releases the core
 * from the boot ROM's wfi. Marking them NOLOAD keeps them allocated but
 * out of the ELF image, so only real code and data are transferred.
 */

OUTPUT_ARCH( "riscv" )
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;
  .text : { *(.text) }
  . = ALIGN(0x1000);
  .tohost : { *(.tohost) }
  . = ALIGN(0x1000);
  .page_table : { *(.page_table) }
  .data : { *(.data) }
  .user_stack (NOLOAD) : { *(.user_stack) }
  .kernel_data : { *(.kernel_data) }
  .kernel_stack (NOLOAD) : { *(.kernel_stack) }
  .bss : { *(.bss) }
  _end = .;
}
