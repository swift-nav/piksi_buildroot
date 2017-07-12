################################################################################
#
# sbp_rtcm3_bridge
#
################################################################################

SBP_RTCM3_BRIDGE_VERSION = 0.1
SBP_RTCM3_BRIDGE_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_rtcm3_bridge/sbp_rtcm3_bridge"
SBP_RTCM3_BRIDGE_SITE_METHOD = local
SBP_RTCM3_BRIDGE_DEPENDENCIES = czmq libsbp libpiksi

define SBP_RTCM3_BRIDGE_BUILD_CMDS_DEFAULT
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef
ifeq ($(BR2_BUILD_TESTS),y)
define SBP_RTCM3_BRIDGE_BUILD_CMDS_TESTS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) test
endef
endif
define SBP_RTCM3_BRIDGE_BUILD_CMDS
	$(SBP_RTCM3_BRIDGE_BUILD_CMDS_DEFAULT)
	$(SBP_RTCM3_BRIDGE_BUILD_CMDS_TESTS)
endef

define SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT
	$(INSTALL) -D -m 0755 $(@D)/src/sbp_rtcm3_bridge $(TARGET_DIR)/usr/bin
endef
ifeq ($(BR2_BUILD_TESTS),y)
define SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/test_sbp_rtcm3_bridge $(TARGET_DIR)/usr/bin
endef
endif
ifeq ($(BR2_RUN_TESTS),y)
define SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_TESTS_RUN
	chroot $(TARGET_DIR) test_sbp_rtcm3_bridge
endef
endif
define SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS
	$(SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT)
	$(SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_TESTS_INSTALL)
	$(SBP_RTCM3_BRIDGE_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

$(eval $(generic-package))
