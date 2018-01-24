################################################################################
#
# libnmea2000
#
################################################################################

LIBNMEA2000_VERSION = ecd56bcf57f90b860759626b9a248d9e05e7ba50
LIBNMEA2000_SITE = https://github.com/ttlappalainen/NMEA2000.git
LIBNMEA2000_SITE_METHOD = git
LIBNMEA2000_INSTALL_STAGING = YES

define LIBNMEA2000_INSTALL_TARGET_CMDS
    for i in $(@D)/src/*.so; do $(INSTALL) $$i $(TARGET_DIR)/usr/lib/; done
endef

define LIBNMEA2000_INSTALL_STAGING_CMDS
    for i in $(@D)/src/*.so; do $(INSTALL) $$i $(STAGING_DIR)/usr/lib/; done
    for i in $(@D)/src/*.h; do $(INSTALL) $$i $(STAGING_DIR)/usr/include/; done
endef

$(eval $(cmake-package))
