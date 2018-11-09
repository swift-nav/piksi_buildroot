################################################################################
#
# cell_modem_daemon
#
################################################################################

CELL_MODEM_DAEMON_VERSION = 0.1
CELL_MODEM_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/cell_modem_daemon"
CELL_MODEM_DAEMON_SITE_METHOD = local
CELL_MODEM_DAEMON_DEPENDENCIES = libuv libsbp libpiksi

define CELL_MODEM_DAEMON_USERS
	celld -1 celld -1 * - - -
endef

define CELL_MODEM_DAEMON_BUILD_CMDS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define CELL_MODEM_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/cell_modem_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${CELL_MODEM_DAEMON_SITE}/overlay"

$(eval $(generic-package))
