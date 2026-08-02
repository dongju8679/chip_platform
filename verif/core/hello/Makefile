COMMON_DIR = ../common
include $(COMMON_DIR)/Makefile

LDFLAGS = -T $(COMMON_DIR)/link.ld -nostdlib

hello.elf: $(COMMON_DIR)/start.S main.c $(COMMON_DIR)/link.ld
	$(CC) $(CFLAGS) -I$(COMMON_DIR) \
	  -o $@ $(COMMON_DIR)/start.S main.c \
	  $(LDFLAGS)

run: hello.elf
	$(SIMULATOR) +permissive +verbose \
	  +max-cycles=1000000 \
	  +permissive-off hello.elf

all: hello.elf
