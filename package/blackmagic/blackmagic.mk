################################################################################
#
# blackmagic
#
################################################################################

BLACKMAGIC_SITE = https://github.com/swift-nav/blackmagic
BLACKMAGIC_VERSION = 7c1c7099881548cd96b5834ed0bbd3d9b0cab925
BLACKMAGIC_SITE_METHOD = git

define BLACKMAGIC_BUILD_CMDS
    $(MAKE) -j1 CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src
endef

define BLACKMAGIC_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/blackmagic $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
