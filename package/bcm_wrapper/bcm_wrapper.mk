################################################################################
#
# bcm_wrapper
#
################################################################################

BCM_WRAPPER_PREFIX = bcm_wrapper
BCM_WRAPPER_SOURCE = $(call pbr_s3_src,$(BCM_WRAPPER_PREFIX))

BCM_WRAPPER_SITE = $(DL_DIR)
BCM_WRAPPER_SITE_METHOD = file

define BCM_WRAPPER_USERS
	bcmd -1 bcmd -1 * - - -
endef

define BCM_WRAPPER_INSTALL_TARGET_CMDS
	( $(foreach l,$(BCM_WRAPPER_LIBS),$(INSTALL) $(@D)/$(strip $l) $(TARGET_DIR)/usr/lib/ &&) \
		exit 0 || exit 1 )
	$(INSTALL) $(@D)/bcm_wrapper $(TARGET_DIR)/usr/bin/bcm_wrapper
endef

BCM_WRAPPER_OVERLAY = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/bcm_wrapper/overlay"

ifeq ($(BR2_PACKAGE_BCM_WRAPPER),y)
BR2_ROOTFS_OVERLAY += "${BCM_WRAPPER_OVERLAY}"
endif

$(eval $(generic-package))
