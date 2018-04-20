################################################################################
#
# ports_daemon
#
################################################################################

PORTS_DAEMON_VERSION = 0.1
PORTS_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/ports_daemon"
PORTS_DAEMON_SITE_METHOD = local
PORTS_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

define PORTS_DAEMON_USERS
	portsd -1 portsd -1 * - - -
endef

ifeq ($(BR2_BUILD_TESTS),y)
define PORTS_DAEMON_BUILD_TESTS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/ports_daemon test
endef
define PORTS_DAEMON_INSTALL_TESTS
	$(INSTALL) -D -m 0755 $(@D)/ports_daemon/test/run_ports_daemon_test $(TARGET_DIR)/usr/bin
endef
PORTS_DAEMON_DEPENDENCIES += gtest
endif
ifeq ($(BR2_RUN_TESTS),y)
define PORTS_DAEMON_RUN_TESTS
	sudo chroot $(TARGET_DIR) run_ports_daemon_test
endef
endif

define PORTS_DAEMON_BUILD_CMDS
  CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" LTO_PLUGIN="$(LTO_PLUGIN)" \
	  $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/ports_daemon src
	$(PORTS_DAEMON_BUILD_TESTS)
endef

define PORTS_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/ports_daemon/src/ports_daemon $(TARGET_DIR)/usr/bin
	$(PORTS_DAEMON_INSTALL_TESTS)
	$(PORTS_DAEMON_RUN_TESTS)
endef

BR2_ROOTFS_OVERLAY += "${PORTS_DAEMON_SITE}/overlay"

$(eval $(generic-package))
