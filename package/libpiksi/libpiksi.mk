################################################################################
#
# libpiksi
#
################################################################################

LIBPIKSI_VERSION = 0.1
LIBPIKSI_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/libpiksi/libpiksi"
LIBPIKSI_SITE_METHOD = local
LIBPIKSI_DEPENDENCIES = libuv czmq libsbp
LIBPIKSI_INSTALL_STAGING = YES

define LIBPIKSI_BUILD_CMDS
  CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" LTO_PLUGIN="$(LTO_PLUGIN)" \
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD)  -C $(@D) all
endef

define LIBPIKSI_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/libpiksi.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/src/libpiksi.a $(STAGING_DIR)/usr/lib
    $(INSTALL) -d -m 0755 $(STAGING_DIR)/usr/include/libpiksi
    $(INSTALL) -D -m 0644 $(@D)/include/libpiksi/*.h \
                          $(STAGING_DIR)/usr/include/libpiksi
endef

define LIBPIKSI_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/libpiksi.so* $(TARGET_DIR)/usr/lib
endef

$(eval $(generic-package))
