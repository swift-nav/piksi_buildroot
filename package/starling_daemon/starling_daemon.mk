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

define STARLING_DAEMON_USERS
	strlngd -1 strlngd -1 * - - -
endef

STARLING_DAEMON_PRE_DOWNLOAD_HOOKS += STARLING_DAEMON_PRE_DOWNLOAD_FIXUP

STARLING_DAEMON_LIBS = \
	libfec.so \
	libstarling.so \
	libswiftnav-common.so \
	libswiftnav.so

STARLING_DAEMON_SRC_NAME = \
	piksi-multi-linux-starling

STARLING_DAEMON_DST_NAME = \
	starlingd

BR2_STRIP_EXCLUDE_FILES += $(STARLING_DAEMON_LIBS)

define STARLING_DAEMON_INSTALL_TARGET_CMDS
	( $(foreach l,$(STARLING_DAEMON_LIBS),$(INSTALL) $(@D)/$(strip $l) $(TARGET_DIR)/usr/lib/ &&) \
		exit 0 || exit 1 )
	$(INSTALL) $(@D)/$(strip $(STARLING_DAEMON_SRC_NAME)) $(TARGET_DIR)/usr/bin/$(STARLING_DAEMON_DST_NAME)
endef

STARLING_DAEMON_OVERLAY = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/starling_daemon/overlay"
BR2_ROOTFS_OVERLAY += "${STARLING_DAEMON_OVERLAY}"

$(eval $(generic-package))

endif # ($(BR2_BUILD_STARLING_DAEMON),y)
