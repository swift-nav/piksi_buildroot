################################################################################
#
# piksi_system_daemon
#
################################################################################

PIKSI_SYSTEM_DAEMON_VERSION = 0.1
PIKSI_SYSTEM_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/piksi_system_daemon/piksi_system_daemon"
PIKSI_SYSTEM_DAEMON_SITE_METHOD = local
PIKSI_SYSTEM_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

define PIKSI_SYSTEM_DAEMON_USERS
	piksisysd -1 piksisysd -1 * - - - Piksi system daemon
	cellmond -1 cellmond -1 * - - - Piksi system daemon, cell modem monitor process
	adapt_tty -1 tty -1 * - - - uart0 adapter
	adapt_tcpc0 -1 adapt_tcpc0 -1 * - - - tcp client 0 adapter
	adapt_tcpc1 -1 adapt_tcpc1 -1 * - - - tcp client 1 adapter
	adapt_tcps0 -1 adapt_tcps0 -1 * - - - tcp server 0 adapter
	adapt_tcps1 -1 adapt_tcps1 -1 * - - - tcp server 1 adapter
	adapt_udps0 -1 adapt_tcps0 -1 * - - - udp server 0 adapter
	adapt_udps1 -1 adapt_tcps1 -1 * - - - udp server 1 adapter
	adapt_udpc0 -1 adapt_tcpc0 -1 * - - - udp client 0 adapter
	adapt_udpc1 -1 adapt_tcpc1 -1 * - - - udp client 1 adapter
endef

ifeq ($(BR2_BUILD_TESTS),y)
	PIKSI_SYSTEM_DAEMON_DEPENDENCIES += gtest
endif

ifeq ($(BR2_BUILD_TESTS),y)
define PIKSI_SYSTEM_DAEMON_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
endef
define PIKSI_SYSTEM_DAEMON_INSTALL_TARGET_CMDS_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/run_piksi_system_daemon_test $(TARGET_DIR)/usr/bin
endef
endif

ifeq ($(BR2_RUN_TESTS),y)
define PIKSI_SYSTEM_DAEMON_INSTALL_TARGET_CMDS_TESTS_RUN
	sudo chroot $(TARGET_DIR) run_piksi_system_daemon_test
endef
endif

define PIKSI_SYSTEM_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
		$(PIKSI_SYSTEM_DAEMON_BUILD_CMDS_TESTS)
endef

define PIKSI_SYSTEM_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/piksi_system_daemon $(TARGET_DIR)/usr/bin
		$(PIKSI_SYSTEM_DAEMON_INSTALL_TARGET_CMDS_TESTS_INSTALL)
		$(PIKSI_SYSTEM_DAEMON_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

$(eval $(generic-package))
