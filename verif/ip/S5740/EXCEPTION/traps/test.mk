# test.mk - shared HAL modules used by EXCEPTION/traps
#
# exceptions : the exception dispatch layer (Platform/Common/Src/exceptions.c)
# trap_entry : its assembly entry point (a .S, hence a separate list)
# boot       : boot_check() - boot state self-check
# data_init  : used by boot.c to obtain the .bss boundary symbols
TEST_HAL_MODULES = exceptions boot data_init
TEST_HAL_ASM     = trap_entry
