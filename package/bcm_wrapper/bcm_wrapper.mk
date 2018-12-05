################################################################################
#
# bcm_wrapper
#
################################################################################

ifeq ($(BR2_PACKAGE_BCM_WRAPPER),y)

BCM_WRAPPER_VERSION = v0.3.13
BCM_WRAPPER_PREFIX = bcm_wrapper
BCM_WRAPPER_ASSET = bcm_wrapper.tar.gz
BCM_WRAPPER_S3 = $(call pbr_s3_url,$(BCM_WRAPPER_PREFIX),$(BCM_WRAPPER_VERSION),$(BCM_WRAPPER_ASSET))
BCM_WRAPPER_SOURCE = $(call pbr_s3_src,$(BCM_WRAPPER_PREFIX),$(BCM_WRAPPER_VERSION),$(BCM_WRAPPER_ASSET))

BCM_WRAPPER_SITE = $(DL_DIR)
BCM_WRAPPER_SITE_METHOD = file

define BCM_WRAPPER_PRE_DOWNLOAD_FIXUP
	$(call pbr_s3_cp,$(BCM_WRAPPER_S3),$(BCM_WRAPPER_SITE),$(BCM_WRAPPER_SOURCE))
endef

define BCM_WRAPPER_USERS
	bcmd -1 bcmd -1 * - - -
endef

BCM_WRAPPER_PRE_DOWNLOAD_HOOKS += BCM_WRAPPER_PRE_DOWNLOAD_FIXUP

#BCM_WRAPPER_LIBS = \
#	libfec.so \
#	libstarling.so \
#	libstarling-shim.so \
#	libstarling-integration.so \
#	libswiftnav.so \
#
#BCM_WRAPPER_SRC_NAME = \
#	bcm_wrapper
#
#BCM_WRAPPER_DST_NAME = \
#	starlingd
#
#BR2_STRIP_EXCLUDE_FILES += \
#	$(BCM_WRAPPER_LIBS) \
#	$(BCM_WRAPPER_DST_NAME)

define BCM_WRAPPER_INSTALL_TARGET_CMDS
	( $(foreach l,$(BCM_WRAPPER_LIBS),$(INSTALL) $(@D)/$(strip $l) $(TARGET_DIR)/usr/lib/ &&) \
		exit 0 || exit 1 )
	$(INSTALL) $(@D)/bcm_wrapper $(TARGET_DIR)/usr/bin/bcm_wrapper
endef

BCM_WRAPPER_OVERLAY = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/bcm_wrapper/overlay"
BR2_ROOTFS_OVERLAY += "${BCM_WRAPPER_OVERLAY}"

$(eval $(generic-package))

endif
