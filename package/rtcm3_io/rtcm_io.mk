################################################################################
#
# rtcm3_io
#
################################################################################

RTCM3_IO_VERSION = 0.1
RTCM3_IO_SITE = "${BR2_EXTERNAL}/package/rtcm3_io/src"
RTCM3_IO_SITE_METHOD = local

define RTCM3_IO_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define RTCM3_IO_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/rtcm3_io $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
