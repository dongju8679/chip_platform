COMMON_DIR = ../common
include $(COMMON_DIR)/Makefile
LDFLAGS = -T $(COMMON_DIR)/link.ld -nostdlib

interrupt.elf: $(COMMON_DIR)/start.S main.c $(COMMON_DIR)/link.ld
	$(CC) $(CFLAGS) -I$(COMMON_DIR) -o $@ $(COMMON_DIR)/start.S main.c $(LDFLAGS)

run: interrupt.elf
	$(SIMULATOR) +permissive +verbose +max-cycles=2000000 +permissive-off interrupt.elf

all: interrupt.elf
