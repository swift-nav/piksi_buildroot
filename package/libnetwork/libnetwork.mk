################################################################################
#
# libnetwork
#
################################################################################

LIBNETWORK_VERSION = 0.1
LIBNETWORK_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/libnetwork/src"
LIBNETWORK_SITE_METHOD = local
LIBNETWORK_DEPENDENCIES = libuv libsbp libpiksi libcurl
LIBNETWORK_INSTALL_STAGING = YES

define LIBNETWORK_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) all
endef

define LIBNETWORK_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libnetwork.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libnetwork.a $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0644 $(@D)/libnetwork.h $(STAGING_DIR)/usr/include
endef

define LIBNETWORK_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libnetwork.so* $(TARGET_DIR)/usr/lib
endef

$(eval $(generic-package))
