include $(sort $(wildcard $(BR2_EXTERNAL_piksi_buildroot_PATH)/package/*/*.mk))

include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/aws-setup.mk
include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/test-support.mk
include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/warnings.mk

ifneq ($(BR2_DISABLE_LTO),)

$(info *** Disabling compiler LTO pass... ***)

BR2_TARGET_OPTIMIZATION := $(filter-out,$(BR2_TARGET_OPTIMIZATION),-flto)
BR2_TARGET_CFLAGS := $(filter-out,$(BR2_TARGET_CFLAGS),-flto)
BR2_TARGET_LDFLAGS := $(filter-out,$(BR2_TARGET_LDFLAGS),-flto)

else

include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/lto.mk

endif
