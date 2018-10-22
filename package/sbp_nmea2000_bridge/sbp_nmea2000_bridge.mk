################################################################################
#
# sbp_nmea2000_bridge
#
################################################################################

SBP_NMEA2000_BRIDGE_VERSION = 0.1
SBP_NMEA2000_BRIDGE_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_nmea2000_bridge/src"
SBP_NMEA2000_BRIDGE_SITE_METHOD = local
SBP_NMEA2000_BRIDGE_DEPENDENCIES = libnmea2000 libsbp libpiksi libsocketcan libuv nanomsg_custom

define SBP_NMEA2000_BRIDGE_BUILD_CMDS
	$(INSTALL) $(@D)/NMEA2000_SocketCAN.h $(STAGING_DIR)/usr/include/
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_NMEA2000_BRIDGE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/sbp_nmea2000_bridge $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
