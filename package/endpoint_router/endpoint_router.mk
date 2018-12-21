################################################################################
#
# endpoint_router
#
################################################################################

ENDPOINT_ROUTER_VERSION = 0.1
ENDPOINT_ROUTER_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/endpoint_router/endpoint_router"
ENDPOINT_ROUTER_SITE_METHOD = local
ENDPOINT_ROUTER_DEPENDENCIES = libuv libsbp libpiksi libyaml
ENDPOINT_ROUTER_INSTALL_STAGING = YES

ifeq    ($(BR2_BUILD_TESTS),y) ##

ENDPOINT_ROUTER_DEPENDENCIES += gtest

define ENDPOINT_ROUTER_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
endef

define ENDPOINT_ROUTER_INSTALL_TARGET_CMDS_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/test_endpoint_router $(TARGET_DIR)/usr/bin
endef

ifeq    ($(BR2_RUN_TESTS),y) ####
define ENDPOINT_ROUTER_INSTALL_TARGET_CMDS_TESTS_RUN
	{ TEST_DATA_DIR=$(ENDPOINT_ROUTER_SITE)/test \
    LD_LIBRARY_PATH=$(TARGET_DIR)/usr/lib \
			valgrind --track-origins=yes --leak-check=full --error-exitcode=1 \
				$(TARGET_DIR)/usr/bin/test_endpoint_router; }
endef
endif # ($(BR2_RUN_TESTS),y) ####

endif # ($(BR2_BUILD_TESTS),y) ##

define ENDPOINT_ROUTER_BUILD_CMDS
    CFLAGS="$(TARGET_CFLAGS)" \
			$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) all
		$(ENDPOINT_ROUTER_BUILD_CMDS_TESTS)
endef

define ENDPOINT_ROUTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/endpoint_router $(TARGET_DIR)/usr/bin
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/endpoint_router
		$(ENDPOINT_ROUTER_INSTALL_TARGET_CMDS_TESTS_INSTALL)
		$(ENDPOINT_ROUTER_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

define ENDPOINT_ROUTER_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/endpoint_router $(STAGING_DIR)/usr/bin
    $(INSTALL) -d -m 0755 $(STAGING_DIR)/etc/endpoint_router
endef

$(eval $(generic-package))
