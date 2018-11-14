################################################################################
#
# query_config
#
################################################################################

QUERY_CONFIG_VERSION = 0.1
QUERY_CONFIG_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/query_config/src"
QUERY_CONFIG_SITE_METHOD = local
QUERY_CONFIG_DEPENDENCIES = libuv libsbp libpiksi

define QUERY_CONFIG_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define QUERY_CONFIG_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/query_config $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
