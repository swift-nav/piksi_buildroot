################################################################################
#
# libpiksi
#
################################################################################

LIBPIKSI_VERSION = 0.1
LIBPIKSI_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/libpiksi/libpiksi"
LIBPIKSI_SITE_METHOD = local
LIBPIKSI_DEPENDENCIES = libuv libsbp busybox libsettings

ifeq ($(BR2_BUILD_TESTS),y)
LIBPIKSI_DEPENDENCIES += gtest valgrind
endif

LIBPIKSI_INSTALL_STAGING = YES

ifeq ($(BR2_BUILD_TESTS),y)
UNIT_TEST_CFLAGS = -DUNIT_TEST_WORKAROUND
endif

define LIBPIKSI_BUILD_CMDS_DEFAULT
  CFLAGS="$(TARGET_CFLAGS) $(UNIT_TEST_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" LTO_PLUGIN="$(LTO_PLUGIN)" \
		PBR_CC_WARNINGS="$(PBR_CC_WARNINGS)" \
    	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD)  -C $(@D) all
endef
ifeq ($(BR2_BUILD_TESTS),y)
define LIBPIKSI_BUILD_CMDS_TESTS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" \
		PBR_CC_WARNINGS="$(PBR_CC_WARNINGS)" PBR_CXX_WARNINGS=$(PBR_CXX_WARNINGS) \
		-C $(@D) test
endef
endif
define LIBPIKSI_BUILD_CMDS
	$(LIBPIKSI_BUILD_CMDS_DEFAULT)
	$(LIBPIKSI_BUILD_CMDS_TESTS)
endef

define LIBPIKSI_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/libpiksi.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/src/libpiksi.a $(STAGING_DIR)/usr/lib
    $(INSTALL) -d -m 0755 $(STAGING_DIR)/usr/include/libpiksi
    $(INSTALL) -D -m 0644 $(@D)/include/libpiksi/*.h \
                          $(STAGING_DIR)/usr/include/libpiksi
endef

define LIBPIKSI_INSTALL_TARGET_CMDS_DEFAULT
    $(INSTALL) -D -m 0755 $(@D)/src/libpiksi.so* $(TARGET_DIR)/usr/lib
endef
ifeq ($(BR2_BUILD_TESTS),y)
define LIBPIKSI_INSTALL_TARGET_CMDS_TESTS_INSTALL
	$(INSTALL) -D -m 0755 $(@D)/test/run_libpiksi_tests $(TARGET_DIR)/usr/bin
	$(INSTALL) -D -m 0755 $(@D)/test/run_libpiksi_str_tests $(TARGET_DIR)/usr/bin
endef
endif

ifeq ($(BR2_RUN_TESTS),y)
LIBPIKSI_INSTALL_TARGET_CMDS_TESTS_RUN = \
	$(call pbr_proot_valgrind_test,run_libpiksi_tests) && \
		$(call pbr_proot_test,run_libpiksi_str_tests)
endif

define LIBPIKSI_INSTALL_TARGET_CMDS
	$(LIBPIKSI_INSTALL_TARGET_CMDS_DEFAULT)
	$(LIBPIKSI_INSTALL_TARGET_CMDS_TESTS_INSTALL)
	$(LIBPIKSI_INSTALL_TARGET_CMDS_TESTS_RUN)
endef

$(eval $(generic-package))
