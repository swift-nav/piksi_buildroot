################################################################################
#
# sbp_rtcm3_bridge
#
################################################################################

SBP_RTCM3_BRIDGE_VERSION = 0.1
SBP_RTCM3_BRIDGE_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_rtcm3_bridge/src"
SBP_RTCM3_BRIDGE_SITE_METHOD = local
SBP_RTCM3_BRIDGE_DEPENDENCIES = libuv czmq libsbp libpiksi gnss_convertors

define SBP_RTCM3_BRIDGE_USERS
	br_rtcm3 -1 br_rtcm3 -1 * - - -
endef

define SBP_RTCM3_BRIDGE_BUILD_CMDS_DEFAULT
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_RTCM3_BRIDGE_BUILD_CMDS
	$(SBP_RTCM3_BRIDGE_BUILD_CMDS_DEFAULT)
endef

define SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT
	$(INSTALL) -D -m 0755 $(@D)/sbp_rtcm3_bridge $(TARGET_DIR)/usr/bin
endef

define SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS
	$(SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT)
endef

$(eval $(generic-package))
