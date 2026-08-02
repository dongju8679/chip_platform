# CMN Application to be included in the binary

CMN_APPLICATION += \
Init \
Intr \
Ipc

VPATH += $(addprefix $(app_cmn_dir)/, $(addsuffix /Src, $(CMN_APPLICATION)))
incs += $(addprefix -I$(app_cmn_dir)/, $(addsuffix /Inc, $(CMN_APPLICATION)))

# Set custom DEFINES if necessary
