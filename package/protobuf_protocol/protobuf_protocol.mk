################################################################################
#
# protobuf_protocol
#
################################################################################

PROTOBUF_PROTOCOL_VERSION = 0.1
PROTOBUF_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/protobuf_protocol/src"
PROTOBUF_PROTOCOL_SITE_METHOD = local
PROTOBUF_PROTOCOL_DEPENDENCIES = 
PROTOBUF_PROTOCOL_INSTALL_STAGING = YES

define PROTOBUF_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define PROTOBUF_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libprotobuf_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libprotobuf_protocol.a $(STAGING_DIR)/usr/lib
endef

define PROTOBUF_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -D -m 0755 $(@D)/libprotobuf_protocol.so*                           \
                          $(TARGET_DIR)/usr/lib/zmq_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/zmq_router
    $(INSTALL) -D -m 0755 $(@D)/protobuf_router.yml $(TARGET_DIR)/etc/zmq_router
endef

$(eval $(generic-package))
