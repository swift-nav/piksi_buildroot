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

define PIKSI_INS_PRE_BUILD1
	echo '>>> *** WARNING: Piksi INS was enabled, but image is not protected! ***'
endef

PIKSI_INS_PRE_BUILD_HOOKS += PIKSI_INS_PRE_BUILD1

endif # ! ($(BR2_BUILD_RELEASE_PROTECTED),y)

$(info >>> Piksi INS is enabled, packaging with current image)

## The piksi_ins version is managed with piksi-releases
#include ${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/piksi_ins_version.mk

PIKSI_INS_VERSION = 3f5acfdbc0363780bf23594ce0b111939a95890c
PIKSI_INS_SITE = git@github.com:swift-nav/pose_daemon_wrapper.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_GIT_SUBMODULES = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = \
	libuv libsbp libpiksi libsettings eigen host-build_tools pfw_welcome

BR2_ROOTFS_OVERLAY += "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/overlay"

define PIKSI_INS_POST_INSTALL
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/build_tools/install_wrapper.sh" \
		$(HOST_DIR) $(@D) $(TARGET_DIR)
endef

PIKSI_INS_POST_INSTALL_TARGET_HOOKS += PIKSI_INS_POST_INSTALL

$(eval $(cmake-package))

else

ifeq      ($(BR2_BUILD_PIKSI_INS),y)

define PIKSI_INS_PRE_BUILD2
	echo '>>> *** WARNING: Piksi INS was enabled, but access to project failed! ***'
endef

PIKSI_INS_PRE_BUILD_HOOKS += PIKSI_INS_PRE_BUILD2

endif

PIKSI_INS_VERSION = 1.0
PIKSI_INS_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/empty"
PIKSI_INS_SITE_METHOD = local
PIKSI_INS_DEPENDENCIES =
BR2_ROOTFS_OVERLAY += "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_ins/overlay"

$(eval $(generic-package))

endif # ($(BR2_HAS_PIKSI_INS),y)
