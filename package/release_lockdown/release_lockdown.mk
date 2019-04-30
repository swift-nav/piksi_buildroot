################################################################################
#
# release_lockdown
#
################################################################################

RELEASE_LOCKDOWN_VERSION = 0.1
RELEASE_LOCKDOWN_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/release_lockdown"
RELEASE_LOCKDOWN_SITE_METHOD = local

define RELEASE_LOCKDOWN_INSTALL_TARGET_CMDS
  @echo '>>>' Image lockdown package installed...
endef

ifeq ($(BR2_PACKAGE_RELEASE_LOCKDOWN),y)
BR2_ROOTFS_OVERLAY += "${RELEASE_LOCKDOWN_SITE}/overlay"
endif

$(eval $(generic-package))
