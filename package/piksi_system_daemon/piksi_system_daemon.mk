################################################################################
#
# piksi_system_daemon
#
################################################################################

PIKSI_SYSTEM_DAEMON_VERSION = 0.1
PIKSI_SYSTEM_DAEMON_SITE = "${BR2_EXTERNAL}/package/piksi_system_daemon/src"
PIKSI_SYSTEM_DAEMON_SITE_METHOD = local
PIKSI_SYSTEM_DAEMON_DEPENDENCIES = czmq libsbp libsbp_zmq libsbp_settings

define PIKSI_SYSTEM_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define PIKSI_SYSTEM_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/piksi_system_daemon $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
