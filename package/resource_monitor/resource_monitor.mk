################################################################################
#
# resource_monitor
#
################################################################################

RESOURCE_MONITOR_VERSION = 0.1
RESOURCE_MONITOR_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/resource_monitor/resource_monitor"
RESOURCE_MONITOR_SITE_METHOD = local
RESOURCE_MONITOR_DEPENDENCIES = libuv libsbp libpiksi

ifeq ($(BR2_BUILD_TESTS),y)
  RESOURCE_MONITOR_DEPENDENCIES += gtest valgrind
endif

define RESOURCE_MONITOR_USERS
	resmond -1 resmond -1 * - - -
endef

ifeq  ($(BR2_BUILD_TESTS),y)

define RESOURCE_MONITOR_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
endef

define RESOURCE_MONITOR_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/run_resource_monitor_test $(TARGET_DIR)/usr/bin
	find $(@D)/test/data -type f -exec $(INSTALL) -D -m 0755 {} $(TARGET_DIR)/data/resource_monitor \;
endef

endif #  BR2_BUILD_TESTS

ifeq ($(BR2_RUN_TESTS),y)

RESOURCE_MONITOR_TESTS_RUN = $(call pbr_proot_valgrind_test,run_resource_monitor_test)

endif

define RESOURCE_MONITOR_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/resource_monitor $(TARGET_DIR)/usr/bin
	$(INSTALL) -D -m 0755 $(@D)/src/extrace $(TARGET_DIR)/usr/bin
  $(RESOURCE_MONITOR_TESTS_INSTALL)
  $(RESOURCE_MONITOR_TESTS_RUN)
endef

define RESOURCE_MONITOR_BUILD_CMDS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" \
		-C $(@D)/src all
  $(RESOURCE_MONITOR_BUILD_CMDS_TESTS)
endef

BR2_ROOTFS_OVERLAY += "${RESOURCE_MONITOR_SITE}/overlay"

$(eval $(generic-package))
