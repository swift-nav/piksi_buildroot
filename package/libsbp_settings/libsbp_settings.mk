################################################################################
#
# libsbp_settings
#
################################################################################

LIBSBP_SETTINGS_VERSION = 0.1
LIBSBP_SETTINGS_SITE = "${BR2_EXTERNAL}/package/libsbp_settings/src"
LIBSBP_SETTINGS_SITE_METHOD = local
LIBSBP_SETTINGS_DEPENDENCIES = libsbp
LIBSBP_SETTINGS_INSTALL_STAGING = YES

define LIBSBP_SETTINGS_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define LIBSBP_SETTINGS_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libsbp_settings.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libsbp_settings.a $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0644 $(@D)/sbp_settings.h $(STAGING_DIR)/usr/include
endef

define LIBSBP_SETTINGS_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libsbp_settings.so* $(TARGET_DIR)/usr/lib
endef

$(eval $(generic-package))
