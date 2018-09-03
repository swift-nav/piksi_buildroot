################################################################################
#
# standalone_file_logger
#
################################################################################

STANDALONE_FILE_LOGGER_VERSION = 0.1
STANDALONE_FILE_LOGGER_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/standalone_file_logger/standalone_file_logger"
STANDALONE_FILE_LOGGER_SITE_METHOD = local
STANDALONE_FILE_LOGGER_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi
ifeq ($(BR2_BUILD_TESTS),y)
	STANDALONE_FILE_LOGGER_DEPENDENCIES += gtest
endif

define STANDALONE_FILE_LOGGER_USERS
	stndfl -1 stndfl -1 * - - -
endef

define STANDALONE_FILE_LOGGER_BUILD_CMDS_DEFAULT
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) all
endef
ifeq ($(BR2_BUILD_TESTS),y)
define STANDALONE_FILE_LOGGER_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) test
endef
endif
define STANDALONE_FILE_LOGGER_BUILD_CMDS
	$(STANDALONE_FILE_LOGGER_BUILD_CMDS_DEFAULT)
	$(STANDALONE_FILE_LOGGER_BUILD_CMDS_TESTS)
endef

define STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS_DEFAULT
	$(INSTALL) -D -m 0755 $(@D)/src/standalone_file_logger $(TARGET_DIR)/usr/bin
endef
ifeq ($(BR2_BUILD_TESTS),y)
define STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/test_standalone_file_logger $(TARGET_DIR)/usr/bin
endef
endif
ifeq ($(BR2_RUN_TESTS),y)
define STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS_TESTS_RUN
	sudo chroot $(TARGET_DIR) test_standalone_file_logger
endef
endif
define STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS
	$(STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS_DEFAULT)
	$(STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS_TESTS_INSTALL)
	$(STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

BR2_ROOTFS_OVERLAY += "${STANDALONE_FILE_LOGGER_SITE}/overlay"

$(eval $(generic-package))
