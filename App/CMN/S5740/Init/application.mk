#BMARK file for $bmark

s5720_init_c_src = \
rfm_init.c

s5720_init_riscv_src = \

s5720_init_c_objs = $(patsubst %.c, %.o, $(s5720_init_c_src))
s5720_init_riscv_objs = $(patsubst %.S, %.o, $(s5720_init_riscv_src))

bmarks_objects += $(s5720_init_c_objs) $(s5720_init_riscv_objs)

junk += $(s5720_init_c_objs) $(s5720_init_riscv_objs)
