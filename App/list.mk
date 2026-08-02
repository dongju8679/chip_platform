# Application to be included in the binary
#

ATE ?= false

app_cmn_dir = $(src_dir)/App/CMN/$(TARGET)
app_l1_dir = $(src_dir)/App/L1/$(TARGET)
app_rf_dir = $(src_dir)/App/RF/$(TARGET)

ifeq ($(ATE), true)
  include $(app_rf_dir)/list.mk
  APPS += $(addprefix RF/$(TARGET)/, $(RF_APPLICATION))
else
ifneq ($(wildcard $(app_cmn_dir)),)
  include $(app_cmn_dir)/list.mk
  APPS += $(addprefix CMN/$(TARGET)/, $(CMN_APPLICATION))
endif
ifneq ($(wildcard $(app_l1_dir)),)
  include $(app_l1_dir)/list.mk
  APPS += $(addprefix L1/$(TARGET)/, $(L1_APPLICATION))
endif
ifneq ($(wildcard $(app_rf_dir)),)
  include $(app_rf_dir)/list.mk
  APPS += $(addprefix RF/$(TARGET)/, $(RF_APPLICATION))
endif
endif

# Set custom DEFINES if necessary
#
ifeq ($(ATE), true)
  CUSTOM_DEFINE += \
                   -DATE
endif
