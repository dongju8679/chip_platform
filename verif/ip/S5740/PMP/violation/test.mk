# test.mk - shared HAL modules used by PMP/violation
#
# pmp        : the PMP configuration API (Platform/Common/Src/pmp.c)
# exceptions : the exception layer that receives and judges the violation traps
# trap_entry : its assembly entry point
# (boot/data_init are unused in this case and are not linked - keeps the image
#  smaller)
TEST_HAL_MODULES = pmp exceptions
TEST_HAL_ASM     = trap_entry
