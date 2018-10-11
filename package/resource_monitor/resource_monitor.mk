################################################################################
#
# resource_monitor
#
################################################################################

RESOURCE_MONITOR_VERSION = 0.1
RESOURCE_MONITOR_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/resource_monitor"
RESOURCE_MONITOR_SITE_METHOD = local
RESOURCE_MONITOR_DEPENDENCIES = libuv nanomsg_custom libsbp libpiksi

define RESOURCE_MONITOR_USERS
	resmond -1 resmond -1 * - - -
endef

define RESOURCE_MONITOR_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/resource_monitor $(TARGET_DIR)/usr/bin
endef

define RESOURCE_MONITOR_BUILD_CMDS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef



$(eval $(generic-package))
