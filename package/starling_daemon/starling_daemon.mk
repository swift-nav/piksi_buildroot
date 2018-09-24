################################################################################
#
# starling_daemon
#
################################################################################

ifeq    ($(BR2_BUILD_STARLING_DAEMON),y)

STARLING_DAEMON_VERSION = v0.1.1
STARLING_DAEMON_PREFIX = starling_daemon
STARLING_DAEMON_ASSET = piksi-multi-linux.deploy.tar.gz
STARLING_DAEMON_S3 = $(call pbr_s3_url,$(STARLING_DAEMON_PREFIX),$(STARLING_DAEMON_ASSET))
STARLING_DAEMON_SOURCE = $(call pbr_s3_src,$(STARLING_DAEMON_PREFIX),$(STARLING_DAEMON_ASSET))

STARLING_DAEMON_SITE = $(DL_DIR)
STARLING_DAEMON_SITE_METHOD = file

define STARLING_DAEMON_PRE_DOWNLOAD_FIXUP
	$(call pbr_s3_cp,$(STARLING_DAEMON_S3),$(STARLING_DAEMON_SITE),$(STARLING_DAEMON_SOURCE))
endef

STARLING_DAEMON_PRE_DOWNLOAD_HOOKS += STARLING_DAEMON_PRE_DOWNLOAD_FIXUP

define STARLING_DAEMON_INSTALL_TARGET_CMDS
	cd $(@D) && $(INSTALL) *.so /usr/lib/
	cd $(@D) && $(INSTALL) piksi-multi-linux-starling /usr/bin
endef

BR2_ROOTFS_OVERLAY += "${STARLING_DAEMON_SITE}/overlay"

$(eval $(generic-package))

endif # ($(BR2_BUILD_STARLING_DAEMON),y)
