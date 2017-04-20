################################################################################
#
# standalone_file_logger
#
################################################################################

STANDALONE_FILE_LOGGER_VERSION = 0.1
STANDALONE_FILE_LOGGER_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/standalone_file_logger/src"
STANDALONE_FILE_LOGGER_SITE_METHOD = local
STANDALONE_FILE_LOGGER_DEPENDENCIES = czmq libsbp libpiksi

define STANDALONE_FILE_LOGGER_BUILD_CMDS
    $(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) all
endef

define STANDALONE_FILE_LOGGER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/standalone_file_logger $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
