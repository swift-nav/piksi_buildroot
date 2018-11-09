################################################################################
#
# sbp_fileio_daemon
#
################################################################################

SBP_FILEIO_DAEMON_VERSION = 0.1
SBP_FILEIO_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_fileio_daemon/sbp_fileio_daemon"
SBP_FILEIO_DAEMON_SITE_METHOD = local
SBP_FILEIO_DAEMON_DEPENDENCIES = libuv libsbp libpiksi

ifeq ($(BR2_BUILD_TESTS),y)
	SBP_FILEIO_DEPENDENCIES += gtest
endif

define SBP_FILEIO_DAEMON_USERS
	fileio -1 fileio -1 * - - -
endef

define SBP_FILEIO_DAEMON_BUILD_CMDS_DEFAULT
	LTO_PLUGIN="$(LTO_PLUGIN)" \
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

ifeq ($(BR2_BUILD_TESTS),y)
define SBP_FILEIO_DAEMON_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
endef
endif

define SBP_FILEIO_DAEMON_BUILD_CMDS
	$(SBP_FILEIO_DAEMON_BUILD_CMDS_DEFAULT)
	$(SBP_FILEIO_DAEMON_BUILD_CMDS_TESTS)
endef

define SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS_DEFAULT
    $(INSTALL) -D -m 0755 $(@D)/src/sbp_fileio_daemon $(TARGET_DIR)/usr/bin
endef

ifeq ($(BR2_BUILD_TESTS),y)
define SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS_TESTS
	$(INSTALL) -D -m 0755 $(@D)/test/run_sbp_fileio_daemon_tests $(TARGET_DIR)/usr/bin
	sudo mkdir -p $(TARGET_DIR)/fake_data
	sudo mkdir -p $(TARGET_DIR)/fake_persist/blah
	sudo chroot $(TARGET_DIR) run_sbp_fileio_daemon_tests
endef
endif

define SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS
	$(SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS_DEFAULT)
	$(SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS_TESTS)
endef

$(eval $(generic-package))
