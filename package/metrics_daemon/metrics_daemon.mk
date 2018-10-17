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

ifeq ($(BR2_BUILD_TESTS),y)
  define METRICS_DAEMON_BUILD_CMDS_TESTS
    $(info >>> *** Buidling metrics daemon test***)
    $(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
  endef
  define METRICS_DAEMON_INSTALL_TARGET_CMDS_TESTS_INSTALL
    $(INSTALL) -D -m 0755 $(@D)/test/run_metrics_daemon_test $(TARGET_DIR)/usr/bin
  endef
endif

ifeq ($(BR2_RUN_TESTS),y)
define METRICS_DAEMON_INSTALL_TARGET_CMDS_TESTS_RUN
	{ cd $(TARGET_DIR); \
	  export PROOT_NO_SECCOMP=1; \
		export _VALGRIND_LIB=$$PWD/../host/x86_64-buildroot-linux-gnu/sysroot/usr/lib/valgrind; \
		test -d $$_VALGRIND_LIB || exit 1; \
		PATH=/bin:/usr/bin:/sbin:/usr/sbin \
		  proot -b $$PWD/../build -b $$_VALGRIND_LIB:/usr/lib/valgrind -R . \
			valgrind \
				--track-origins=yes \
				--leak-check=full \
				--error-exitcode=1 \
				/usr/bin/run_metrics_daemon_test; \
	}
endef
endif

define METRICS_DAEMON_USERS
  metricsd -1 metricsd -1 * - - -
endef

define METRICS_DAEMON_INSTALL_TARGET_CMDS
  $(INSTALL) -D -m 0755 $(@D)/src/metrics_daemon $(TARGET_DIR)/usr/bin
  $(METRICS_DAEMON_INSTALL_TARGET_CMDS_TESTS_INSTALL)
  $(METRICS_DAEMON_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

define METRICS_DAEMON_BUILD_CMDS
  $(info >>> *** Buidling metrics daemon ***)
  $(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
  $(METRICS_DAEMON_BUILD_CMDS_TESTS)
endef

BR2_ROOTFS_OVERLAY += "${METRICS_DAEMON_SITE}/overlay"
$(eval $(generic-package))
