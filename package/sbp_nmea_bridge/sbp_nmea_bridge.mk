################################################################################
#
# sbp_nmea_bridge
#
################################################################################

SBP_NMEA_BRIDGE_VERSION = 0.1
SBP_NMEA_BRIDGE_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_nmea_bridge/src"
SBP_NMEA_BRIDGE_SITE_METHOD = local
SBP_NMEA_BRIDGE_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi gnss_convertors

define SBP_NMEA_BRIDGE_USERS
	br_nmea -1 br_nmea -1 * - - -
endef

define SBP_NMEA_BRIDGE_BUILD_CMDS_DEFAULT
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_NMEA_BRIDGE_BUILD_CMDS
	$(SBP_NMEA_BRIDGE_BUILD_CMDS_DEFAULT)
endef

define SBP_NMEA_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT
	$(INSTALL) -D -m 0755 $(@D)/sbp_nmea_bridge $(TARGET_DIR)/usr/bin
endef

define SBP_NMEA_BRIDGE_INSTALL_TARGET_CMDS
	$(SBP_NMEA_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT)
endef

$(eval $(generic-package))
