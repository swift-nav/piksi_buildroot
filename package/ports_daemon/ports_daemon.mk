################################################################################
#
# ports_daemon
#
################################################################################

PORTS_DAEMON_VERSION = 0.1
PORTS_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/ports_daemon"
PORTS_DAEMON_SITE_METHOD = local
PORTS_DAEMON_DEPENDENCIES = czmq libsbp libpiksi libcurl libnetwork

define PORTS_DAEMON_USERS
	portsd -1 portsd -1 * - - -
endef

define PORTS_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define PORTS_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/ports_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${PORTS_DAEMON_SITE}/overlay"

$(eval $(generic-package))
