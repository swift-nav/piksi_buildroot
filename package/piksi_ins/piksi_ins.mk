#############################################################
#
# piksi_ins
#
#############################################################

define PIKSI_INS_USERS
	piksi_ins -1 devmem -1 * - - -
endef

ifeq      ($(BR2_HAS_PIKSI_INS),y)
ifneq     ($(BR2_BUILD_RELEASE_PROTECTED),y)

$(info >>> *** WARNING: Piksi INS was enabled, but image is not protected! ***)

endif # ! ($(BR2_BUILD_RELEASE_PROTECTED),y)

$(info >>> Piksi INS is enabled, packaging with current image)

PIKSI_INS_VERSION = v2.2.0 # Version 2.2.0 pushed on Nov 14, 2018
PIKSI_INS_SITE = git@github.com:carnegieroboticsllc/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = \
	libuv libsbp libpiksi eigen host-build_tools pfw_welcome

BR2_ROOTFS_OVERLAY += "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/overlay"

define PIKSI_INS_POST_INSTALL
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/build_tools/install_wrapper.sh" \
		$(HOST_DIR) $(@D) $(TARGET_DIR)
endef

PIKSI_INS_POST_INSTALL_TARGET_HOOKS += PIKSI_INS_POST_INSTALL

$(eval $(cmake-package))

else

ifeq      ($(BR2_BUILD_PIKSI_INS),y)
$(info >>> *** WARNING: Piksi INS was enabled, but access to project failed! ***)
endif

PIKSI_INS_VERSION = 1.0
PIKSI_INS_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/empty"
PIKSI_INS_SITE_METHOD = local
PIKSI_INS_DEPENDENCIES =
BR2_ROOTFS_OVERLAY += "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/overlay"

$(eval $(generic-package))

endif # ($(BR2_HAS_PIKSI_INS),y)
