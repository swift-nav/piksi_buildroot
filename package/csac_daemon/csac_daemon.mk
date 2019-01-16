################################################################################
#
# csac_daemon
#
################################################################################

CSAC_DAEMON_VERSION = 0.1
CSAC_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/csac_daemon"
CSAC_DAEMON_SITE_METHOD = local
CSAC_DAEMON_DEPENDENCIES = libuv libsbp libpiksi libsettings

define CSAC_DAEMON_BUILD_CMDS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define CSAC_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/csac_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${CSAC_DAEMON_SITE}/overlay"

$(eval $(generic-package))
