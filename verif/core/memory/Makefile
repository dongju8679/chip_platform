COMMON_DIR = ../common
include $(COMMON_DIR)/Makefile
LDFLAGS = -T $(COMMON_DIR)/link.ld -nostdlib

memory.elf: $(COMMON_DIR)/start.S main.c $(COMMON_DIR)/link.ld
	$(CC) $(CFLAGS) -I$(COMMON_DIR) -o $@ $(COMMON_DIR)/start.S main.c $(LDFLAGS)

run: memory.elf
	$(SIMULATOR) +permissive +verbose +max-cycles=1000000 +permissive-off memory.elf

all: memory.elf
