################################################################################
#
# llvm_obfuscator
#
################################################################################

HOST_LLVM_OBFUSCATOR_VERSION = v18
HOST_LLVM_OBFUSCATOR_SOURCE = $(HOST_LLVM_OBFUSCATOR_VERSION)/llvm-obfuscator-arm-x86.txz
HOST_LLVM_OBFUSCATOR_SITE = https://github.com/swift-nav/llvm-obfuscator-arm/releases/download
HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL = llvm-obfuscator-$(HOST_LLVM_OBFUSCATOR_VERSION).tar.xz

LLVM_OBF_TARBALL_SRC = $(DL_DIR)/$(shell basename $(HOST_LLVM_OBFUSCATOR_SOURCE))
LLVM_OBF_TARBALL_DST = $(DL_DIR)/$(HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL) 

define HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_FIXUP
	if ! [ -e $(LLVM_OBF_TARBALL_DST) ] || [ $(LLVM_OBF_TARBALL_SRC) -nt $(LLVM_OBF_TARBALL_DST) ]; then \
		mv -v $(LLVM_OBF_TARBALL_SRC) $(LLVM_OBF_TARBALL_DST); \
	fi
	$(eval HOST_LLVM_OBFUSCATOR_SOURCE=$(HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL))
endef

HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_HOOKS += HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_FIXUP
SYSROOT = $(shell find $(HOST_DIR) -name 'sysroot' | grep -v llvm-obf)

define HOST_LLVM_OBFUSCATOR_INSTALL_CMDS
	mkdir -p $(HOST_DIR)/opt/llvm-obfuscator
	rsync -az $(@D)/opt/llvm-obfuscator/ $(HOST_DIR)/opt/llvm-obfuscator/
endef

LLVM_OBF_CC      = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-clang
LLVM_OBF_CXX     = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-clang++
LLVM_OBF_AR      = $(HOST_DIR)/opt/llvm-obfuscator/bin/llvm-ar
LLVM_OBF_STRIP   = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-strip
LLVM_OBF_OBJCOPY = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-objcopy

LLVM_OBF_HOSTCC  = $(HOST_DIR)/opt/llvm-obfuscator/bin/clang
LLVM_OBF_HOSTCXX = $(HOST_DIR)/opt/llvm-obfuscator/bin/clang++

$(eval $(host-generic-package))

