################################################################################
#
# ota_daemon
#
################################################################################

OTA_DAEMON_VERSION = 0.1
OTA_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/ota_daemon"
OTA_DAEMON_SITE_METHOD = local
OTA_DAEMON_DEPENDENCIES = libuv libsbp libpiksi libcurl libnetwork json-c
OTA_DAEMON_INSTALL_STAGING = YES

define OTA_DAEMON_USERS
	otad -1 otad -1 * - - -
endef

define OTA_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src all
endef

define OTA_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/ota_daemon $(TARGET_DIR)/usr/bin
endef

ifeq ($(BR2_PACKAGE_OTA_DAEMON),y)
BR2_ROOTFS_OVERLAY += "${OTA_DAEMON_SITE}/overlay"
endif

$(eval $(generic-package))
