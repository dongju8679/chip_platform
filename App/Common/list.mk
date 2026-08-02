# list.mk - common App module list (chip-independent)
COMMON_APP_DIRS := \
  App/CMN/common/Init \
  App/CMN/common/Intr \
  App/CMN/common/Main

COMMON_APP_INC := \
  App/CMN/common/Init/Inc \
  App/CMN/common/Intr/Inc \
  App/CMN/common/Main/Inc

COMMON_APP_SRC := \
  App/CMN/common/Init/Src/app_init.c \
  App/CMN/common/Intr/Src/app_intr.c \
  App/CMN/common/Main/Src/app_main.c
