OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
  SRAM   (rwx) : ORIGIN = 0x80000000, LENGTH = 16K
  TOHOST (rw)  : ORIGIN = 0x80001000, LENGTH = 64
}

SECTIONS
{
  .text : {
    *(.text.init)
    *(.text .text.*)
  } > SRAM

  .rodata : ALIGN(4) {
    *(.rodata .rodata.*)
  } > SRAM

  .data : ALIGN(4) {
    *(.data .data.*)
  } > SRAM

  .bss (NOLOAD) : ALIGN(4) {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } > SRAM

  . = ALIGN(4);
  _stack_top = ORIGIN(SRAM) + LENGTH(SRAM);

  .tohost : {
    *(.tohost)
  } > TOHOST
}
