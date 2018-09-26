################################################################################
#
# starling_daemon
#
################################################################################

ifeq    ($(BR2_BUILD_STARLING_DAEMON),y)

STARLING_DAEMON_VERSION = v0.2.5
STARLING_DAEMON_PREFIX = starling_daemon
STARLING_DAEMON_ASSET = piksi-multi-linux.deploy.tar.gz
STARLING_DAEMON_S3 = $(call pbr_s3_url,$(STARLING_DAEMON_PREFIX),$(STARLING_DAEMON_VERSION),$(STARLING_DAEMON_ASSET))
STARLING_DAEMON_SOURCE = $(call pbr_s3_src,$(STARLING_DAEMON_PREFIX),$(STARLING_DAEMON_VERSION),$(STARLING_DAEMON_ASSET))

STARLING_DAEMON_SITE = $(DL_DIR)
STARLING_DAEMON_SITE_METHOD = file

define STARLING_DAEMON_PRE_DOWNLOAD_FIXUP
	$(call pbr_s3_cp,$(STARLING_DAEMON_S3),$(STARLING_DAEMON_SITE),$(STARLING_DAEMON_SOURCE))
endef

STARLING_DAEMON_PRE_DOWNLOAD_HOOKS += STARLING_DAEMON_PRE_DOWNLOAD_FIXUP

define STARLING_DAEMON_INSTALL_TARGET_CMDS
	cd $(@D) && $(INSTALL) *.so $(TARGET_DIR)/usr/lib/
	cd $(@D) && $(INSTALL) piksi-multi-linux-starling $(TARGET_DIR)/usr/bin
endef

STARLING_DAEMON_OVERLAY = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/starling_daemon/overlay"
BR2_ROOTFS_OVERLAY += "${STARLING_DAEMON_OVERLAY}"

$(eval $(generic-package))

endif # ($(BR2_BUILD_STARLING_DAEMON),y)
