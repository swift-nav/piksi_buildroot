################################################################################
#
# sbp_rtcm3_bridge
#
################################################################################

SBP_RTCM3_BRIDGE_VERSION = 0.1
SBP_RTCM3_BRIDGE_SITE = "${BR2_EXTERNAL}/package/sbp_rtcm3_bridge/src"
SBP_RTCM3_BRIDGE_SITE_METHOD = local
SBP_RTCM3_BRIDGE_DEPENDENCIES = czmq libsbp

define SBP_RTCM3_BRIDGE_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/sbp_rtcm3_bridge $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
