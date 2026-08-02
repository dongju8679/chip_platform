# test.mk - UART/printf build options
#
# This case is where the stdout path (printf -> _write -> UART0) is verified.
# Since the switch to newlib stdio (USE_NEWLIB_STDIO=1) it also checks the
# %f/%e/%g soft-float conversions here.
#
# newlib-nano's printf omits the float conversion code by default, so it must be
# pulled in explicitly with -u _printf_float (see the "%f support" section of
# Makefile.VERIF).
# It is a link-level decision and adds ~20 KB when enabled -> only turn it on in
# cases that need it.
#
# rv32imac has no F extension, so %f is handled in soft float
# (__adddf3/__divdf3/__gdtoa ...) - no FPU instructions are emitted.
TEST_PRINTF_FLOAT = 1
