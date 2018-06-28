################################################################################
#
# endpoint_router
#
################################################################################

ENDPOINT_ROUTER_VERSION = 0.1
ENDPOINT_ROUTER_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/endpoint_router/endpoint_router"
ENDPOINT_ROUTER_SITE_METHOD = local
ENDPOINT_ROUTER_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi libyaml

ENDPOINT_ROUTER_INSTALL_STAGING = YES

define ENDPOINT_ROUTER_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) CFLAGS="$(TARGET_CFLAGS)" -C $(@D) all
endef

define ENDPOINT_ROUTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/endpoint_router $(TARGET_DIR)/usr/bin
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/endpoint_router
endef

define ENDPOINT_ROUTER_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/endpoint_router $(STAGING_DIR)/usr/bin
    $(INSTALL) -d -m 0755 $(STAGING_DIR)/etc/endpoint_router
endef

$(eval $(generic-package))
