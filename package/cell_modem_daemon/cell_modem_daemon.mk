################################################################################
#
# cell_modem_daemon
#
################################################################################

CELL_MODEM_DAEMON_VERSION = 0.1
CELL_MODEM_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/cell_modem_daemon/src"
CELL_MODEM_DAEMON_SITE_METHOD = local
CELL_MODEM_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

define CELL_MODEM_DAEMON_BUILD_CMDS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define CELL_MODEM_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/cell_modem_daemon $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))

