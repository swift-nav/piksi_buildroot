################################################################################
#
# llvm_obfuscator
#
################################################################################

HOST_LLVM_OBFUSCATOR_VERSION = v1
HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL = llvm-obfuscated-arm-$(HOST_LLVM_OBFUSCATOR_VERSION).tgz
HOST_LLVM_OBFUSCATOR_SOURCE = 5043255
HOST_LLVM_OBFUSCATOR_SITE = https://$(GITHUB_TOKEN):@api.github.com/repos/swift-nav/llvm-obfuscator-arm/releases/assets
HOST_LLVM_OBFUSCATOR_METHOD = wget
HOST_LLVM_OBFUSCATOR_DL_OPTS = --auth-no-challenge --header='Accept:application/octet-stream'

define HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_FIXUP
	if ! [ -e $(DL_DIR)/$(HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL) ]; then \
		mv -v $(DL_DIR)/5043255 $(DL_DIR)/$(HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL); fi
	$(eval HOST_LLVM_OBFUSCATOR_SOURCE=$(HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL))
endef

HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_HOOKS += HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_FIXUP

define HOST_LLVM_OBFUSCATOR_BUILD_CMDS
	mkdir -p $(HOST_DIR)/opt/llvm-obfuscator
	rsync -az $(@D)/opt/llvm-obfuscator/ $(HOST_DIR)/opt/llvm-obfuscator/
endef

$(eval $(host-generic-package))
