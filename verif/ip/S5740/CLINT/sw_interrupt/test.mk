# test.mk - CLINT/sw_interrupt build options
#
# This case is deliberately kept as the reference "rt-dev copy-paste original".
#   The test folder carries its own Src/sw.c and Inc/sw.h.
#   The shared HAL has files of the same name (Platform/Common/Src/sw.c,
#   Inc/sw.h), so the opt-in below deliberately creates a "conflict".
#
#   The override rule in Makefile.VERIF resolves it:
#     if the test folder has a .c of the same name, the shared one is dropped
#     from the link.
#   The rule worked if the build log prints:
#     [hal] test overrides common: sw.c
#   To check:
#     make -f Implementation/Makefile.VERIF src_dir=. F=CLINT SF=sw_interrupt \
#          TRACK=baremetal show-src
#
#   In other words, this single line is a regression device that proves on every
#   build that "a copy-pasted rt-dev test does not break when its file names
#   collide with the shared HAL".
#   Do not delete it.
TEST_HAL_MODULES = sw
