# Chipset/TinyRocket - the slot for the existing baremetal/common = TinyRocket BSP
Drop your originals in here:
  start.S      (boot: set up mtvec -> register init -> stack)
  link.ld      (TCM 16KB @ 0x80000000~0x80003FFF, tohost symbols)
  hal.h        (uart_puts etc., the tohost macros)
hal_shim/ : S5740 peripheral mocks, for verifying MCU logic on TinyRocket
