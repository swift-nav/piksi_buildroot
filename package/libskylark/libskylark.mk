################################################################################
#
# libskylark
#
################################################################################

LIBSKYLARK_VERSION = 0.1
LIBSKYLARK_SITE = "${BR2_EXTERNAL}/package/libskylark/src"
LIBSKYLARK_SITE_METHOD = local
LIBSKYLARK_DEPENDENCIES = czmq libsbp libpiksi libcurl
LIBSKYLARK_INSTALL_STAGING = YES

define LIBSKYLARK_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define LIBSKYLARK_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libskylark.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libskylark.a $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0644 $(@D)/libskylark.h $(STAGING_DIR)/usr/include
endef

define LIBSKYLARK_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libskylark.so* $(TARGET_DIR)/usr/lib
endef

$(eval $(generic-package))
