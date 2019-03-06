################################################################################
#
# j1939_protocol
#
################################################################################

J1939_PROTOCOL_VERSION = 0.1
J1939_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/j1939_protocol/src"
J1939_PROTOCOL_SITE_METHOD = local
J1939_PROTOCOL_DEPENDENCIES =
J1939_PROTOCOL_INSTALL_STAGING = YES

define J1939_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) all
endef

define J1939_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libj1939_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libj1939_protocol.a $(STAGING_DIR)/usr/lib
endef

define J1939_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -D -m 0755 $(@D)/libj1939_protocol.so*                         \
                          $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/endpoint_router
    $(INSTALL) -D -m 0755 $(@D)/j1939_router.yml $(TARGET_DIR)/etc/endpoint_router
endef

$(eval $(generic-package))
