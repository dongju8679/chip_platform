# test.mk - CLINT/timer_interrupt build options
#
# This case is the one that was "migrated to use the shared HAL".
#   before: the test folder carried Src/timer.c + Inc/timer.h
#   after : they were promoted to Platform/Common/Src/timer.c + Inc/timer.h and
#           deleted from the test folder. Only the opt-in remains here.
# #include "timer.h" in CLINT_timer_interrupt.c now resolves to the shared
# header (-I order in CFLAGS: verif/<...>/Inc -> Platform/Common/Inc).
#
# For the opposite, paired case see CLINT/sw_interrupt/test.mk (copy-paste
# original kept + override).
TEST_HAL_MODULES = timer
