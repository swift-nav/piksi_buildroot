include $(sort $(wildcard $(BR2_EXTERNAL_piksi_buildroot_PATH)/package/*/*.mk))

include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/aws-setup.mk
include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/test-support.mk
include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/warnings.mk

ifeq ($(VARIANT),host)
PBR_DISABLE_LTO := y
endif

ifneq ($(PBR_DISABLE_LTO),)

$(info >>> **************************************)
$(info >>> *** Disabling compiler LTO pass... ***)
$(info >>> **************************************)

BR2_TARGET_OPTIMIZATION := $(filter-out,$(BR2_TARGET_OPTIMIZATION),-flto)
BR2_TARGET_CFLAGS := $(filter-out,$(BR2_TARGET_CFLAGS),-flto)
BR2_TARGET_LDFLAGS := $(filter-out,$(BR2_TARGET_LDFLAGS),-flto)

else

include $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/lto.mk

endif

ifneq ($(IN_PIKSI_SHELL),)
# Work around issue with libtool scripts on NixOS "discovering" system
#   libraries instead of the ones built by buildroot.
TARGET_LDFLAGS += -L$(HOST_DIR)/arm-buildroot-linux-gnueabihf/sysroot/usr/lib
endif
