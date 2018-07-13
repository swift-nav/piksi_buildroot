################################################################################
#
# endpoint_adapter
#
################################################################################

ENDPOINT_ADAPTER_VERSION = 0.1
ENDPOINT_ADAPTER_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/endpoint_adapter/src"
ENDPOINT_ADAPTER_SITE_METHOD = local
ENDPOINT_ADAPTER_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi

define ENDPOINT_ADAPTER_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define ENDPOINT_ADAPTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/endpoint_adapter $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
