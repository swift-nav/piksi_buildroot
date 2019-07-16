#############################################################
#
# piksi_ins
#
#############################################################

define PIKSI_INS_USERS
	piksi_ins -1 devmem -1 * - - -
endef

define PIKSI_INS_PRE_INSTALL_WARNING
	@echo '>>>' Piksi INS is enabled, packaging with current image...
  @if [ "True" != "$$($(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/get-variant-prop $(VARIANT) encrypted)" ]; then \
		echo '>>> *** WARNING: Piksi INS was enabled, but image is not protected! ***'; fi
endef

PIKSI_INS_PRE_INSTALL_TARGET_HOOKS += PIKSI_INS_PRE_INSTALL_WARNING

## The piksi_ins version is managed with piksi-releases
#include ${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/piksi_ins_version.mk

# https://github.com/swift-nav/pose_daemon_wrapper/commits/dzollo/stillness_dz as of 7/15/2019
#
PIKSI_INS_VERSION = 1220443cb40bfa7044ca9f939385e582e06143ee

PIKSI_INS_SITE = git@github.com:swift-nav/pose_daemon_wrapper.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_GIT_SUBMODULES = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = \
	libuv libsbp libswiftnav libpiksi libsettings eigen host-build_tools pfw_welcome

ifeq ($(BR2_PACKAGE_PIKSI_INS),y)
BR2_ROOTFS_OVERLAY += "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/overlay"
endif

define PIKSI_INS_POST_INSTALL
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/build_tools/install_wrapper.sh" \
		$(HOST_DIR) $(@D) $(TARGET_DIR)
endef

PIKSI_INS_POST_INSTALL_TARGET_HOOKS += PIKSI_INS_POST_INSTALL

$(eval $(cmake-package))
