################################################################################
#
# sbp_settings_daemon
#
################################################################################

SBP_SETTINGS_DAEMON_VERSION = 0.1
SBP_SETTINGS_DAEMON_SITE = "${BR2_EXTERNAL}/package/sbp_settings_daemon/src"
SBP_SETTINGS_DAEMON_SITE_METHOD = local
SBP_SETTINGS_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

define SBP_SETTINGS_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_SETTINGS_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/sbp_settings_daemon $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
