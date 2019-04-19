################################################################################
#
# watchdog_daemon
#
################################################################################

ifeq ($(BR2_PACKAGE_WATCHDOG_DAEMON),y)

WATCHDOG_DAEMON_VERSION = 0.1
WATCHDOG_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/watchdog_daemon"
WATCHDOG_DAEMON_SITE_METHOD = local
WATCHDOG_DAEMON_DEPENDENCIES = libuv libsbp libpiksi libcurl libnetwork json-c

define WATCHDOG_DAEMON_USERS
	watchdogd -1 watchdogd -1 * - - -
endef

define WATCHDOG_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define WATCHDOG_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/watchdog_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${WATCHDOG_DAEMON_SITE}/overlay"

$(eval $(generic-package))

endif
