################################################################################
#
# nmea_protocol
#
################################################################################

NMEA_PROTOCOL_VERSION = 0.1
NMEA_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/nmea_protocol/src"
NMEA_PROTOCOL_SITE_METHOD = local
NMEA_PROTOCOL_DEPENDENCIES =
NMEA_PROTOCOL_INSTALL_STAGING = YES

define NMEA_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) all
endef

define NMEA_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libnmea_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libnmea_protocol.a $(STAGING_DIR)/usr/lib
endef

define NMEA_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -D -m 0755 $(@D)/libnmea_protocol.so*                          \
                          $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/endpoint_router
    $(INSTALL) -D -m 0755 $(@D)/nmea_router.yml $(TARGET_DIR)/etc/endpoint_router
endef

$(eval $(generic-package))
