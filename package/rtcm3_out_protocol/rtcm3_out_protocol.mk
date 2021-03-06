################################################################################
#
# rtcm3_out_protocol
#
################################################################################

RTCM3_OUT_PROTOCOL_VERSION = 0.1
RTCM3_OUT_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/rtcm3_out_protocol/src"
RTCM3_OUT_PROTOCOL_SITE_METHOD = local
RTCM3_OUT_PROTOCOL_DEPENDENCIES =
RTCM3_OUT_PROTOCOL_INSTALL_STAGING = YES

define RTCM3_OUT_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) all
endef

define RTCM3_OUT_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/librtcm3_out_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/librtcm3_out_protocol.a $(STAGING_DIR)/usr/lib
endef

define RTCM3_OUT_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -D -m 0755 $(@D)/librtcm3_out_protocol.so*                         \
                          $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/endpoint_router
endef

$(eval $(generic-package))
