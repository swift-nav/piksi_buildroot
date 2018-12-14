################################################################################
#
# nav_daemon
#
################################################################################

NAV_DAEMON_VERSION = 0.1
NAV_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/nav_daemon"
NAV_DAEMON_SITE_METHOD = local
NAV_DAEMON_DEPENDENCIES = libuv libsbp libpiksi

define NAV_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/nav_daemon $(TARGET_DIR)/usr/bin
endef

define NAV_DAEMON_BUILD_CMDS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef

$(eval $(generic-package))
