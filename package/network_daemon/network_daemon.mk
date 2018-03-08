################################################################################
#
# network_daemon
#
################################################################################

NETWORK_DAEMON_VERSION = 0.1
NETWORK_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/network_daemon"
NETWORK_DAEMON_SITE_METHOD = local
NETWORK_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

define NETWORK_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/network_daemon $(TARGET_DIR)/usr/bin
endef

define NETWORK_DAEMON_BUILD_CMDS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef

$(eval $(generic-package))
