################################################################################
#
# DISCOVERY_BROADCASTER
#
################################################################################

DISCOVERY_BROADCASTER_VERSION = 0.1
DISCOVERY_BROADCASTER_SITE = "${BR2_EXTERNAL}/package/discovery_broadcaster/src"
DISCOVERY_BROADCASTER_SITE_METHOD = local
DISCOVERY_BROADCASTER_DEPENDENCIES =

define DISCOVERY_BROADCASTER_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define DISCOVERY_BROADCASTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/discovery_broadcaster $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
