################################################################################
#
# blackmagic
#
################################################################################

BLACKMAGIC_SITE = https://github.com/swift-nav/blackmagic
BLACKMAGIC_VERSION = c0500b2abe29bc62ebe7709eb7c6a46447e9cfdb
BLACKMAGIC_SITE_METHOD = git

define BLACKMAGIC_BUILD_CMDS
    $(MAKE) -j1 CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src
endef

define BLACKMAGIC_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/blackmagic $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
