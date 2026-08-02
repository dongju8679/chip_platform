PREFIX  ?= riscv64-unknown-elf-
CC       = $(PREFIX)gcc
AS       = $(PREFIX)gcc
OBJDUMP  = $(PREFIX)objdump

# TinyRocketConfig: RV32IMAC, no FPU
ARCH    = -march=rv32imac_zicsr_zifencei -mabi=ilp32
CFLAGS  = $(ARCH) -O2 -nostdlib -nostartfiles \
          -ffreestanding -Wall -I.
ASFLAGS = $(ARCH) -nostdlib -nostartfiles
LDFLAGS = -T link.ld -nostdlib

COMMON  = start.S

%.elf: $(COMMON) %.c link.ld
	$(CC) $(CFLAGS) -o $@ $(COMMON) $*.c $(LDFLAGS)

%.dump: %.elf
	$(OBJDUMP) -d $< > $@

clean:
	rm -f *.elf *.dump *.o
