################################################################################
#
# piksi_ins_ref
#
################################################################################

PIKSI_INS_REF_VERSION = v8
PIKSI_INS_REF_SITE = git@github.com:swift-nav/piksi_inertial_ipsec.git
PIKSI_INS_REF_SITE_METHOD = git
PIKSI_INS_REF_DEPENDENCIES = host-llvm_obfuscator

define PIKSI_INS_REF_BUILD_CMDS
	$(MAKE) CC=$(LLVM_OBF_CC) CXX=$(LLVM_OBF_CXX) AR=$(LLVM_OBF_AR) \
				  STRIP=$(LLVM_OBF_STRIP) HOSTCC=$(LLVM_OBF_HOSTCC) \
					HOSTCXX=$(LLVM_OBF_HOSTCXX) OBJCOPY=$(LLVM_OBF_OBJCOPY) \
		-j1 -C $(@D) selfverify
endef

define PIKSI_INS_REF_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/selfverify $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
