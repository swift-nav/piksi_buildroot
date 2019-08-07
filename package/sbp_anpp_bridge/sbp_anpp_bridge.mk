################################################################################
#
# sbp_anpp_bridge
#
################################################################################

SBP_ANPP_BRIDGE_VERSION = 0.1
SBP_ANPP_BRIDGE_SITE = \
       "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_anpp_bridge"
SBP_ANPP_BRIDGE_SITE_METHOD = local
SBP_ANPP_BRIDGE_DEPENDENCIES = libuv libsbp libpiksi libsettings

define SBP_ANPP_BRIDGE_USERS
       br_anpp -1 br_anpp -1 * - - -
endef

define SBP_ANPP_BRIDGE_BUILD_CMDS_DEFAULT
       $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define SBP_ANPP_BRIDGE_BUILD_CMDS
       $(SBP_ANPP_BRIDGE_BUILD_CMDS_DEFAULT)
endef

define SBP_ANPP_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT
       $(INSTALL) -D -m 0755 $(@D)/src/sbp_anpp_bridge $(TARGET_DIR)/usr/bin
endef

define SBP_ANPP_BRIDGE_INSTALL_TARGET_CMDS
       $(SBP_ANPP_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT)
endef

BR2_ROOTFS_OVERLAY += "${SBP_ANPP_BRIDGE_SITE}/overlay"

$(eval $(generic-package))
