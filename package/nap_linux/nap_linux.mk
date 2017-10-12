################################################################################
#
# nap_linux
#
################################################################################

NAP_LINUX_VERSION = 0.1
NAP_LINUX_SITE = \
	"${BR2_EXTERNAL_piksi_buildroot_PATH}/package/nap_linux/src"
NAP_LINUX_SITE_METHOD = local
NAP_LINUX_DEPENDENCIES = host-llvm_obfuscator

OBF_FLAGS = -mllvm -sub -mllvm -bcf -mllvm -fla

define NAP_LINUX_BUILD_CMDS_DEFAULT
	CFLAGS="$(OBF_FLAGS)" $(MAKE) CC=$(LLVM_OBF_CC) CXX=$(LLVM_OBF_CXX) -C $(@D) all
endef

define NAP_LINUX_BUILD_CMDS
	$(NAP_LINUX_BUILD_CMDS_DEFAULT)
endef

define NAP_LINUX_INSTALL_TARGET_CMDS_DEFAULT
	$(INSTALL) -D -m 0755 $(@D)/nap_linux $(TARGET_DIR)/usr/bin
endef

define NAP_LINUX_INSTALL_TARGET_CMDS
	$(NAP_LINUX_INSTALL_TARGET_CMDS_DEFAULT)
endef

$(eval $(generic-package))
