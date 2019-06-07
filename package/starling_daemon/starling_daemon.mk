################################################################################
#
# starling_daemon
#
################################################################################

STARLING_DAEMON_PREFIX = starling_daemon
STARLING_DAEMON_SOURCE = $(call pbr_s3_src,$(STARLING_DAEMON_PREFIX))

STARLING_DAEMON_SITE = $(DL_DIR)
STARLING_DAEMON_SITE_METHOD = file

STARLING_DAEMON_INSTALL_STAGING = YES

define STARLING_DAEMON_USERS
	strlngd -1 strlngd -1 * - - -
endef

STARLING_DAEMON_LIBS = \
	libfec.so \
	libstarling.so \
	libstarling-shim.so \
	libstarling-integration.so \
	libswiftnav.so \

STARLING_DAEMON_SRC_NAME = \
	piksi-multi-linux-starling

STARLING_DAEMON_DST_NAME = \
	starlingd

BR2_STRIP_EXCLUDE_FILES += \
	$(STARLING_DAEMON_LIBS) \
	$(STARLING_DAEMON_DST_NAME)

define STARLING_DAEMON_INSTALL_TARGET_CMDS
	( $(foreach l,$(STARLING_DAEMON_LIBS),$(INSTALL) $(@D)/$(strip $l) $(TARGET_DIR)/usr/lib/ &&) \
		exit 0 || exit 1 )
	$(INSTALL) $(@D)/$(strip $(STARLING_DAEMON_SRC_NAME)) $(TARGET_DIR)/usr/bin/$(STARLING_DAEMON_DST_NAME)
endef

STARLING_DAEMON_OVERLAY = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/starling_daemon/overlay"

ifeq ($(BR2_PACKAGE_STARLING_DAEMON),y)
BR2_ROOTFS_OVERLAY += "${STARLING_DAEMON_OVERLAY}"
endif

$(eval $(generic-package))
