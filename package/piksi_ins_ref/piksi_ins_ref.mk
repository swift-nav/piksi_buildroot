################################################################################
#
# piksi_ins_ref
#
################################################################################

PIKSI_INS_REF_VERSION = v3
PIKSI_INS_REF_SITE = git@github.com:swift-nav/piksi_inertial_ipsec_crl.git
PIKSI_INS_REF_SITE_METHOD = git
PIKSI_INS_REF_DEPENDENCIES = host-llvm_obfuscator

ifeq    ($(BR2_BUILD_TESTS),y)
ifeq    ($(BR2_RUN_TESTS),y)

define PIKSI_INS_REF_BUILD_CMDS_TESTS
	$(MAKE) -j1 -C $(@D) submodules_as_archives
	$(MAKE) CC=$(TARGET_CC) CXX=$(TARGET_CXX) AR=ar STRIP=strip HOSTCC=$(TARGET_CC) \
					HOSTCXX=$(TARGET_CXX) OBJCOPY=objcopy OBF_FLAGS= \
		-j1 -C $(@D) all test
endef

endif # ($(BR2_RUN_TESTS),y)
endif # ($(BR2_BUILD_TESTS),y)

ifneq    ($(BR2_BUILD_TESTS),y)
define PIKSI_INS_REF_BUILD_FOR_TARGET
	$(MAKE) -j1 -C $(@D) submodules_as_archives
	$(MAKE) CC=$(LLVM_OBF_CC) CXX=$(LLVM_OBF_CXX) AR=$(LLVM_OBF_AR) \
				  STRIP=$(LLVM_OBF_STRIP) HOSTCC=$(LLVM_OBF_HOSTCC) \
					HOSTCXX=$(LLVM_OBF_HOSTCXX) OBJCOPY=$(LLVM_OBF_OBJCOPY) \
		-j1 -C $(@D) all
endef
endif # ($(BR2_BUILD_TESTS),y)

define PIKSI_INS_REF_BUILD_CMDS
	$(PIKSI_INS_REF_BUILD_FOR_TARGET)
	$(PIKSI_INS_REF_BUILD_CMDS_TESTS)
endef

define PIKSI_INS_REF_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/selfverify_test $(TARGET_DIR)/usr/bin
	$(INSTALL) -D -m 0755 $(@D)/bin/ip_unlock_test $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
