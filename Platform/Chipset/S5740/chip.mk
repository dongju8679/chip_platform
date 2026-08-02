#source files for s5740

s5740_c_src = \
rom_app.c \
rom_common.c \
chip.c \
usercode.c \
mtvec.c \
ipc.c \
timeout.c \
log_device.c \
pspeedy.c \
exceptions.c \
interface.c \
i2sr.c

s5740_riscv_src = \
crt.S

s5740_c_objs = $(patsubst %.c, %.o, $(s5740_c_src))
s5740_riscv_objs = $(patsubst %.S, %.o, $(s5740_riscv_src))

bmarks_objects += $(s5740_c_objs) $(s5740_riscv_objs)

junk += $(s5740_c_objs) $(s5740_riscv_objs)
