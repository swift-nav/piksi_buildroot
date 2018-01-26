################################################################################
#
# zmq_adapter
#
################################################################################

ZMQ_ADAPTER_VERSION = 0.1
ZMQ_ADAPTER_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/zmq_adapter/zmq_adapter"
ZMQ_ADAPTER_SITE_METHOD = local
ZMQ_ADAPTER_DEPENDENCIES = cmph czmq libsbp libpiksi

ifeq    ($(BR2_BUILD_TESTS),y) ##

ZMQ_ADAPTER_DEPENDENCIES += gtest
ZMQ_ADAPTER_CFLAGS = -g

define ZMQ_ADAPTER_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) test
endef

define ZMQ_ADAPTER_INSTALL_TARGET_CMDS_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/test_zmq_adapter $(TARGET_DIR)/usr/bin
endef

ifeq    ($(BR2_RUN_TESTS),y) ####
define ZMQ_ADAPTER_INSTALL_TARGET_CMDS_TESTS_RUN
	{ PROTOS=$$(mktemp -d); \
		LD_LIBRARY_PATH=$(TARGET_DIR)/usr/lib \
		SETTINGS_WHITELIST_DIR=$(@D)/test/data \
		ADAPTER_PROTOCOL_DIR=$$PROTOS \
			valgrind --leak-check=full --error-exitcode=1 \
				$(TARGET_DIR)/usr/bin/test_zmq_adapter; \
		rm -rf $$PROTOS; }
endef
endif # ($(BR2_RUN_TESTS),y) ####

endif # ($(BR2_BUILD_TESTS),y) ##

define ZMQ_ADAPTER_BUILD_CMDS
    CFLAGS=$(ZMQ_ADAPTER_CFLAGS) \
		$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) \
			-C $(@D) all
		$(ZMQ_ADAPTER_BUILD_CMDS_TESTS)
endef

define ZMQ_ADAPTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/zmq_adapter $(TARGET_DIR)/usr/bin
		$(ZMQ_ADAPTER_INSTALL_TARGET_CMDS_TESTS_INSTALL)
		$(ZMQ_ADAPTER_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

$(eval $(generic-package))
