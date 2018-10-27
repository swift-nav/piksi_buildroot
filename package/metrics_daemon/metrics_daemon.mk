################################################################################
#
# metrics_daemon
#
################################################################################

METRICS_DAEMON_VERSION = 0.1
METRICS_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/metrics_daemon/metrics_daemon"
METRICS_DAEMON_SITE_METHOD = local
METRICS_DAEMON_DEPENDENCIES = json-c libuv nanomsg_custom libsbp libpiksi

ifeq ($(BR2_BUILD_TESTS),y)
  METRICS_DAEMON_DEPENDENCIES += gtest valgrind
endif

ifeq  ($(BR2_BUILD_TESTS),y)

define METRICS_DAEMON_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
endef

define METRICS_DAEMON_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/run_metrics_daemon_test $(TARGET_DIR)/usr/bin
endef

endif #  BR2_BUILD_TESTS

ifeq ($(BR2_RUN_TESTS),y)

METRICS_DAEMON_TESTS_RUN = $(call pbr_proot_valgrind_test,run_metrics_daemon_test)

endif

define METRICS_DAEMON_USERS
  metricsd -1 metricsd -1 * - - -
endef

define METRICS_DAEMON_INSTALL_TARGET_CMDS
  $(INSTALL) -D -m 0755 $(@D)/src/metrics_daemon $(TARGET_DIR)/usr/bin
  $(METRICS_DAEMON_TESTS_INSTALL)
  $(METRICS_DAEMON_TESTS_RUN)
endef

define METRICS_DAEMON_BUILD_CMDS
  $(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" \
		-C $(@D)/src all
  $(METRICS_DAEMON_BUILD_CMDS_TESTS)
endef

BR2_ROOTFS_OVERLAY += "${METRICS_DAEMON_SITE}/overlay"
$(eval $(generic-package))
