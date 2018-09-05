################################################################################
#
# sbp_fileio_daemon
#
################################################################################

SBP_FILEIO_DAEMON_VERSION = 0.1
SBP_FILEIO_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_fileio_daemon/sbp_fileio_daemon"
SBP_FILEIO_DAEMON_SITE_METHOD = local
SBP_FILEIO_DAEMON_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi

ifeq ($(BR2_BUILD_TESTS),y)
	SBP_FILEIO_DEPENDENCIES += gtest
endif

define SBP_FILEIO_DAEMON_USERS
	fio_fw -1 fio_fw -1 * - - -
	fio_ex -1 fio_ex -1 * - - -
endef

define SBP_FILEIO_DAEMON_BUILD_CMDS_DEFAULT
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
    $(INSTALL) -D -m 0755 $(@D)/sbp_fileio_daemon $(TARGET_DIR)/usr/bin
endef

ifeq ($(BR2_BUILD_TESTS),y)
define SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS_TESTS
	$(INSTALL) -D -m 0755 $(@D)/test/run_sbp_fileio_daemon_tests $(TARGET_DIR)/usr/bin
	sudo chroot $(TARGET_DIR) run_sbp_fileio_daemon_tests
endef
endif

define SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS
	$(SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS_DEFAULT)
	$(SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS_TESTS)
endef

$(eval $(generic-package))
