################################################################################
#
# piksi_dev_tools
#
################################################################################

ifeq ($(BR2_BUILD_RELEASE_PROTECTED),)
ifeq ($(BR2_BUILD_RELEASE_OPEN),)
BR2_BUILD_PIKSI_DEV_TOOLS=y
endif
endif

ifneq   ($(BR2_BUILD_PIKSI_DEV_TOOLS),)

$(info >>> Piksi dev tools package enabled...)

PIKSI_DEV_TOOLS_VERSION = 0.1
PIKSI_DEV_TOOLS_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_dev_tools"
PIKSI_DEV_TOOLS_SITE_METHOD = local
PIKSI_DEV_TOOLS_DEPENDENCIES = \
	dropbear blackmagic stress strace host-llvm_vanilla

define PIKSI_DEV_TOOLS_INSTALL_TARGET_CMDS
	@:
endef

$(eval $(generic-package))

else

$(info >>> Piksi dev tools package DISABLED...)

endif # ($(BR2_BUILD_PIKSI_DEV_TOOLS),)
