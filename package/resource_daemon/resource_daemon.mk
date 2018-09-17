################################################################################
#
# resource_daemon
#
################################################################################

RESOURCE_DAEMON_VERSION = 0.1
RESOURCE_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/resource_daemon"
RESOURCE_DAEMON_SITE_METHOD = local
RESOURCE_DAEMON_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi

define RESOURCE_DAEMON_USERS
	resourced -1 resourced -1 * - - -
endef

define RESOURCE_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/resource_daemon $(TARGET_DIR)/usr/bin
endef

define RESOURCE_DAEMON_BUILD_CMDS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef



$(eval $(generic-package))
