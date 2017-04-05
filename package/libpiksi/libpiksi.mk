################################################################################
#
# libpiksi
#
################################################################################

LIBPIKSI_VERSION = 0.1
LIBPIKSI_SITE = "${BR2_EXTERNAL}/package/libpiksi/src"
LIBPIKSI_SITE_METHOD = local
LIBPIKSI_DEPENDENCIES = czmq libsbp
LIBPIKSI_INSTALL_STAGING = YES

define LIBPIKSI_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define LIBPIKSI_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libpiksi.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libpiksi.a $(STAGING_DIR)/usr/lib
    $(INSTALL) -d -m 0755 $(STAGING_DIR)/usr/include/libpiksi
    $(INSTALL) -D -m 0644 $(@D)/*.h $(STAGING_DIR)/usr/include/libpiksi
endef

define LIBPIKSI_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libpiksi.so* $(TARGET_DIR)/usr/lib
endef

$(eval $(generic-package))
