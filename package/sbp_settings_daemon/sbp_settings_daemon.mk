################################################################################
#
# sbp_settings_daemon
#
################################################################################

SBP_SETTINGS_DAEMON_VERSION = 0.1
SBP_SETTINGS_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_settings_daemon/sbp_settings_daemon"
SBP_SETTINGS_DAEMON_SITE_METHOD = local
SBP_SETTINGS_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

ifeq ($(BR2_BUILD_TESTS),y)
	SBP_SETTINGS_DAEMON_DEPENDENCIES += gtest
endif

ifeq ($(BR2_BUILD_TESTS),y)
define SBP_SETTINGS_DAEMON_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
endef
define SBP_SETTINGS_DAEMON_INSTALL_TARGET_CMDS_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/run_sbp_settings_daemon_test $(TARGET_DIR)/usr/bin
endef
endif

ifeq ($(BR2_RUN_TESTS),y)
define SBP_SETTINGS_DAEMON_INSTALL_TARGET_CMDS_TESTS_RUN
	chroot $(TARGET_DIR) run_sbp_settings_daemon_test
endef
endif

define SBP_SETTINGS_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
		$(SBP_SETTINGS_DAEMON_BUILD_CMDS_TESTS)
endef

define SBP_SETTINGS_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/sbp_settings_daemon $(TARGET_DIR)/usr/bin
		$(SBP_SETTINGS_DAEMON_INSTALL_TARGET_CMDS_TESTS_INSTALL)
		$(SBP_SETTINGS_DAEMON_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

$(eval $(generic-package))
