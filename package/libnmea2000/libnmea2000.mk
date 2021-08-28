################################################################################
#
# libnmea2000
#
################################################################################

LIBNMEA2000_VERSION = d471eb504d7e817096172b913d572dc6ea102836
LIBNMEA2000_SITE = https://github.com/ttlappalainen/NMEA2000.git
LIBNMEA2000_SITE_METHOD = git
LIBNMEA2000_INSTALL_STAGING = YES

define LIBNMEA2000_INSTALL_TARGET_CMDS
    for i in $(@D)/src/*.a; do $(INSTALL) $$i $(TARGET_DIR)/usr/lib/; done
endef

define LIBNMEA2000_INSTALL_STAGING_CMDS
    for i in $(@D)/src/*.a; do $(INSTALL) $$i $(STAGING_DIR)/usr/lib/; done
    for i in $(@D)/src/*.h; do $(INSTALL) $$i $(STAGING_DIR)/usr/include/; done
endef

$(eval $(cmake-package))
