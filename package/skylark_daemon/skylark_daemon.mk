################################################################################
#
# skylark_daemon
#
################################################################################

SKYLARK_DAEMON_VERSION = 0.1
SKYLARK_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/skylark_daemon"
SKYLARK_DAEMON_SITE_METHOD = local
SKYLARK_DAEMON_DEPENDENCIES = libuv czmq libsbp libpiksi libcurl libnetwork

define SKYLARK_DAEMON_USERS
	skylarkd -1 skylarkd -1 * - - -
endef

define SKYLARK_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define SKYLARK_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/skylark_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${SKYLARK_DAEMON_SITE}/overlay"

$(eval $(generic-package))
