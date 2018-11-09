################################################################################
#
# orion_daemon
#
################################################################################

ORION_DAEMON_VERSION = 0.1
ORION_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/orion_daemon"
ORION_DAEMON_SITE_METHOD = local
ORION_DAEMON_DEPENDENCIES = libuv libsbp libpiksi gflags protobuf_custom grpc_custom

define ORION_DAEMON_USERS
	oriond -1 oriond -1 * - - -
endef

define ORION_DAEMON_BUILD_CMDS
    $(MAKE) CROSS=$(TARGET_CROSS) CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" \
			-C $(@D)/src all
endef

define ORION_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/orion_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${ORION_DAEMON_SITE}/overlay"

$(eval $(generic-package))
