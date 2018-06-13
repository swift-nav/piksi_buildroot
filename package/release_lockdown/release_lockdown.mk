################################################################################
#
# release_lockdown
#
################################################################################

ifneq ($(BR2_BUILD_RELEASE_PROTECTED),)
BR2_BUILD_RELEASE_LOCKDOWN=y
endif

ifneq ($(BR2_BUILD_RELEASE_OPEN),)
BR2_BUILD_RELEASE_LOCKDOWN=y
endif

ifneq   ($(BR2_BUILD_RELEASE_LOCKDOWN),)

$(info *** Image lockdown package enabled...)

RELEASE_LOCKDOWN_VERSION = 0.1
RELEASE_LOCKDOWN_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/release_lockdown"
RELEASE_LOCKDOWN_SITE_METHOD = local

BR2_ROOTFS_OVERLAY += "${RELEASE_LOCKDOWN_SITE}/overlay"

$(eval $(generic-package))

else

$(info *** Image lockdown package DISABLED...)

endif # ($(BR2_BUILD_RELEASE_LOCKDOWN),)
