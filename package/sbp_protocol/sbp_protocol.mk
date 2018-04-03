################################################################################
#
# sbp_protocol
#
################################################################################

SBP_PROTOCOL_VERSION = 0.1
SBP_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_protocol/src"
SBP_PROTOCOL_SITE_METHOD = local
SBP_PROTOCOL_DEPENDENCIES = libsbp
SBP_PROTOCOL_INSTALL_STAGING = YES

define SBP_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libsbp_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libsbp_protocol.a $(STAGING_DIR)/usr/lib
endef

define SBP_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -D -m 0755 $(@D)/libsbp_protocol.so*                           \
                          $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/zmq_router
    $(INSTALL) -D -m 0755 $(@D)/sbp_router.yml $(TARGET_DIR)/etc/zmq_router
    $(INSTALL) -D -m 0755 $(@D)/sbp_router_smoothpose.yml $(TARGET_DIR)/etc/zmq_router
endef

$(eval $(generic-package))
