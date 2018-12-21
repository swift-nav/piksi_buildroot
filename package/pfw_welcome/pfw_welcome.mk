################################################################################
#
# pfw_welcome
#
################################################################################

PFW_WELCOME_VERSION = 0.1
PFW_WELCOME_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/pfw_welcome/src"
PFW_WELCOME_SITE_METHOD = local
PFW_WELCOME_DEPENDENCIES = libuv libpiksi libsettings

define PFW_WELCOME_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define PFW_WELCOME_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/pfw_welcome $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
