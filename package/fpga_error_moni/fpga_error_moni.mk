################################################################################
#
# fpga_error_moni
#
################################################################################

FPGA_ERROR_MONI_VERSION = 0.1
FPGA_ERROR_MONI_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/fpga_error_moni/src"
FPGA_ERROR_MONI_SITE_METHOD = local
FPGA_ERROR_MONI_DEPENDENCIES = czmq libpiksi

#define NMEA_DAEMON_USERS
#	nmead -1 nmead -1 * - - -
#endef

define FPGA_ERROR_MONI_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define FPGA_ERROR_MONI_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/fpga_error_moni $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
