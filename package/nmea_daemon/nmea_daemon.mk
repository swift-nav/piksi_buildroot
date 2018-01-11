################################################################################
#
# nmea_daemon
#
################################################################################

NMEA_DAEMON_VERSION = 0.1
NMEA_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/nmea_daemon/src"
NMEA_DAEMON_SITE_METHOD = local
NMEA_DAEMON_DEPENDENCIES = czmq libpiksi

define NMEA_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define NMEA_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/nmea_daemon $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
