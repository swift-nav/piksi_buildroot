################################################################################
#
# anpp_protocol
#
################################################################################

ANPP_PROTOCOL_VERSION = 0.1 
ANPP_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/anpp_protocol"
ANPP_PROTOCOL_SITE_METHOD = local
ANPP_PROTOCOL_DEPENDENCIES = libpiksi
ANPP_PROTOCOL_INSTALL_STAGING = YES 

define ANPP_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D)/src all
endef

define ANPP_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/libanpp_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/src/libanpp_protocol.a $(STAGING_DIR)/usr/lib
endef

define ANPP_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -D -m 0755 $(@D)/src/libanpp_protocol.so*                         \
                          $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/endpoint_router
    $(INSTALL) -D -m 0755 $(@D)/src/anpp_router.yml $(TARGET_DIR)/etc/endpoint_router
endef

ifeq ($(BR2_PACKAGE_ANPP_PROTOCOL),y)
BR2_ROOTFS_OVERLAY += "${ANPP_PROTOCOL_SITE}/overlay"
endif

$(eval $(generic-package))
