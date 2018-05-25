################################################################################
#
# piksi_leds
#
################################################################################

PIKSI_LEDS_VERSION = 0.1
PIKSI_LEDS_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_leds/src"
PIKSI_LEDS_SITE_METHOD = local
PIKSI_LEDS_DEPENDENCIES = libuv czmq libsbp libpiksi

define PIKSI_LEDS_USERS
	ledd -1 ledd -1 * - - -
endef

define PIKSI_LEDS_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define PIKSI_LEDS_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/piksi_leds $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
