################################################################################
#
# sbp_j1939_bridge
#
################################################################################

SBP_J1939_BRIDGE_VERSION = 0.1
SBP_J1939_BRIDGE_SITE = \
       "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_j1939_bridge"
SBP_J1939_BRIDGE_SITE_METHOD = local
SBP_J1939_BRIDGE_DEPENDENCIES = libuv libsbp libpiksi libsettings

define SBP_J1939_BRIDGE_USERS
       br_j1939 -1 br_j1939 -1 * - - -
endef

define SBP_J1939_BRIDGE_BUILD_CMDS_DEFAULT
       $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define SBP_J1939_BRIDGE_BUILD_CMDS
       $(SBP_J1939_BRIDGE_BUILD_CMDS_DEFAULT)
endef

define SBP_J1939_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT
       $(INSTALL) -D -m 0755 $(@D)/src/sbp_j1939_bridge $(TARGET_DIR)/usr/bin
endef

define SBP_J1939_BRIDGE_INSTALL_TARGET_CMDS
       $(SBP_J1939_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT)
endef

ifeq ($(BR2_PACKAGE_SBP_J1939_BRIDGE),y)
BR2_ROOTFS_OVERLAY += "${SBP_J1939_BRIDGE_SITE}/overlay"
endif

$(eval $(generic-package))
