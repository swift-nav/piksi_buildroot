################################################################################
#
# common_init
#
################################################################################

COMMON_INIT_VERSION = 0.1
COMMON_INIT_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/common_init"
COMMON_INIT_SITE_METHOD = local

define COMMON_INIT_BUILD_CMDS
    @echo "Building common init..."
endef

define COMMON_INIT_INSTALL_TARGET_CMDS
    @echo "Installing common init..."
endef

define COMMON_INIT_USERS
	ifplugd -1 ifplugd -1 * - - -
	pk_log -1 pk_log -1 * - - -
endef

BR2_ROOTFS_OVERLAY += "${COMMON_INIT_SITE}/overlay"

$(eval $(generic-package))
