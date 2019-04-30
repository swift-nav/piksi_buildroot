################################################################################
#
# piksi_dev_tools
#
################################################################################

PIKSI_DEV_TOOLS_VERSION = 0.1
PIKSI_DEV_TOOLS_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_dev_tools"
PIKSI_DEV_TOOLS_SITE_METHOD = local
PIKSI_DEV_TOOLS_DEPENDENCIES = \
	dropbear blackmagic stress strace host-llvm_vanilla

define PIKSI_DEV_TOOLS_INSTALL_TARGET_CMDS
	@echo '>>>' Piksi dev tools packages installed...
endef

$(eval $(generic-package))
