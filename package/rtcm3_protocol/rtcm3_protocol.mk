################################################################################
#
# rtcm3_protocol
#
################################################################################

RTCM3_PROTOCOL_VERSION = 0.1
RTCM3_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/rtcm3_protocol/src"
RTCM3_PROTOCOL_SITE_METHOD = local
RTCM3_PROTOCOL_DEPENDENCIES =
RTCM3_PROTOCOL_INSTALL_STAGING = YES

define RTCM3_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define RTCM3_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/librtcm3_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/librtcm3_protocol.a $(STAGING_DIR)/usr/lib
endef

define RTCM3_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -D -m 0755 $(@D)/librtcm3_protocol.so*                         \
                          $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/zmq_router
    $(INSTALL) -D -m 0755 $(@D)/rtcm3_router.yml $(TARGET_DIR)/etc/zmq_router
endef

$(eval $(generic-package))
