################################################################################
#
# sbp_protobuf_bridge
#
################################################################################

SBP_PROTO3_BRIDGE_VERSION = 0.1
SBP_PROTO3_BRIDGE_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_protobuf_bridge/src"
SBP_PROTO3_BRIDGE_SITE_METHOD = local
SBP_PROTO3_BRIDGE_DEPENDENCIES = czmq libsbp libpiksi

define SBP_PROTO3_BRIDGE_BUILD_CMDS_DEFAULT
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_PROTO3_BRIDGE_BUILD_CMDS
	$(SBP_PROTO3_BRIDGE_BUILD_CMDS_DEFAULT)
endef

define SBP_PROTO3_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT
	$(INSTALL) -D -m 0755 $(@D)/sbp_protobuf_bridge $(TARGET_DIR)/usr/bin
endef

define SBP_PROTO3_BRIDGE_INSTALL_TARGET_CMDS
	$(SBP_PROTO3_BRIDGE_INSTALL_TARGET_CMDS_DEFAULT)
endef

$(eval $(generic-package))
