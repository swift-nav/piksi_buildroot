################################################################################
#
# health_daemon
#
################################################################################

HEALTH_DAEMON_VERSION = 0.1
HEALTH_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/health_daemon/src"
HEALTH_DAEMON_SITE_METHOD = local
HEALTH_DAEMON_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi libnetwork libcurl

define HEALTH_DAEMON_USERS
	healthd -1 healthd -1 * - - -
endef

define HEALTH_DAEMON_BUILD_CMDS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define HEALTH_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/health_daemon $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
