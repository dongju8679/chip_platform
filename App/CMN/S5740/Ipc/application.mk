#BMARK file for $bmark

s5720_ipc_c_src = \
rfm_ipc_msg.c \
rfm_ipc_handler.c

s5720_ipc_riscv_src = \

s5720_ipc_c_objs = $(patsubst %.c, %.o, $(s5720_ipc_c_src))
s5720_ipc_riscv_objs = $(patsubst %.S, %.o, $(s5720_ipc_riscv_src))

bmarks_objects += $(s5720_ipc_c_objs) $(s5720_ipc_riscv_objs)

junk += $(s5720_ipc_c_objs) $(s5720_ipc_riscv_objs)
