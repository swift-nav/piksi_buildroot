################################################################################
#
# sample_daemon
#
################################################################################

SAMPLE_DAEMON_VERSION = 0.1
SAMPLE_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sample_daemon"
SAMPLE_DAEMON_SITE_METHOD = local
SAMPLE_DAEMON_DEPENDENCIES = libuv libsbp libpiksi
SAMPLE_DAEMON_INSTALL_STAGING = YES

define SAMPLE_DAEMON_USERS
	sampld -1 sampld -1 * - - -
endef

define SAMPLE_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/sample_daemon $(TARGET_DIR)/usr/bin
endef

define SAMPLE_DAEMON_BUILD_CMDS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef

ifeq ($(BR2_PACKAGE_SAMPLE_DAEMON),y)
BR2_ROOTFS_OVERLAY += "${SAMPLE_DAEMON_SITE}/overlay"
endif

$(eval $(generic-package))
