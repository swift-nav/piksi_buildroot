################################################################################
#
# ntrip_daemon
#
################################################################################

NTRIP_DAEMON_VERSION = 0.1
NTRIP_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/ntrip_daemon"
NTRIP_DAEMON_SITE_METHOD = local
NTRIP_DAEMON_DEPENDENCIES = libuv czmq libsbp libpiksi libcurl libnetwork

define NTRIP_DAEMON_USERS
	ntripd -1 ntripd -1 * - - -
endef

define NTRIP_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define NTRIP_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/ntrip_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${NTRIP_DAEMON_SITE}/overlay"

$(eval $(generic-package))
