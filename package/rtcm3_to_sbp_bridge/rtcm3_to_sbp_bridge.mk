################################################################################
#
# rtcm3_to_sbp_bridge
#
################################################################################

RTCM3_TO_SBP_BRIDGE_VERSION = 0.1
RTCM3_TO_SBP_BRIDGE_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/rtcm3_to_sbp_bridge/src"
RTCM3_TO_SBP_BRIDGE_SITE_METHOD = local
RTCM3_TO_SBP_BRIDGE_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi gnss_convertors

define RTCM3_TO_SBP_BRIDGE_USERS
	br_rtcm3tosbp -1 br_rtcm3tosbp -1 * - - -
endef

define RTCM3_TO_SBP_BRIDGE_BUILD_CMDS_DEFAULT
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define RTCM3_TO_SBP_BRIDGE_BUILD_CMDS
	$(RTCM3_TO_SBP_BRIDGE_BUILD_CMDS_DEFAULT)
endef

define RTCM3_TO_SBP_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT
	$(INSTALL) -D -m 0755 $(@D)/rtcm3_to_sbp_bridge $(TARGET_DIR)/usr/bin
endef

define RTCM3_TO_SBP_BRIDGE_INSTALL_TARGET_CMDS
	$(RTCM3_TO_SBP_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT)
endef

$(eval $(generic-package))
