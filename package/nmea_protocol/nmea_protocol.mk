################################################################################
#
# nmea_protocol
#
################################################################################

NMEA_PROTOCOL_VERSION = 0.1
NMEA_PROTOCOL_SITE = "${BR2_EXTERNAL}/package/nmea_protocol/src"
NMEA_PROTOCOL_SITE_METHOD = local
NMEA_PROTOCOL_DEPENDENCIES =
NMEA_PROTOCOL_INSTALL_STAGING = YES

define NMEA_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define NMEA_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libnmea_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libnmea_protocol.a $(STAGING_DIR)/usr/lib
endef

define NMEA_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -D -m 0755 $(@D)/libnmea_protocol.so*                          \
                          $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/zmq_router
    $(INSTALL) -D -m 0755 $(@D)/nmea_router.yml $(TARGET_DIR)/etc/zmq_router
endef

$(eval $(generic-package))
