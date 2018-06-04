################################################################################
#
# piksi_system_daemon
#
################################################################################

PIKSI_SYSTEM_DAEMON_VERSION = 0.1
PIKSI_SYSTEM_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_system_daemon"
PIKSI_SYSTEM_DAEMON_SITE_METHOD = local
PIKSI_SYSTEM_DAEMON_DEPENDENCIES = libuv nanomsg libsbp libpiksi

define PIKSI_SYSTEM_DAEMON_BUILD_CMDS
  CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" LTO_PLUGIN="$(LTO_PLUGIN)" \
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define PIKSI_SYSTEM_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/piksi_system_daemon $(TARGET_DIR)/usr/bin
endef

define PIKSI_SYSTEM_DAEMON_USERS
	piksi_sys -1 piksi_sys -1 * - - -
endef

BR2_ROOTFS_OVERLAY += "${PIKSI_SYSTEM_DAEMON_SITE}/overlay"

$(eval $(generic-package))
