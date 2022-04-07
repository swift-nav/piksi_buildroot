################################################################################
#
# heading_forwarder
#
################################################################################

HEADING_FORWARDER_VERSION = 0.1
HEADING_FORWARDER_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/heading_forwarder/src"
HEADING_FORWARDER_SITE_METHOD = local
HEADING_FORWARDER_DEPENDENCIES = libuv libsbp libpiksi

define HEADING_FORWARDER_USERS
	hdgfwd -1 hdgfwd -1 * - - -
endef

define HEADING_FORWARDER_BUILD_CMDS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define HEADING_FORWARDER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/heading_forwarder $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
