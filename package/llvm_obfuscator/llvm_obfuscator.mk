################################################################################
#
# llvm_obfuscator
#
################################################################################

HOST_LLVM_OBFUSCATOR_VERSION = v14
HOST_LLVM_OBFUSCATOR_SOURCE = $(HOST_LLVM_OBFUSCATOR_VERSION)/llvm-obfuscator-arm-x86.txz
HOST_LLVM_OBFUSCATOR_SITE = https://github.com/swift-nav/llvm-obfuscator-arm/releases/download/
HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL = llvm-obfuscator-$(HOST_LLVM_OBFUSCATOR_VERSION).tar.xz

TARBALL_SRC = $(DL_DIR)/$(shell basename $(HOST_LLVM_OBFUSCATOR_SOURCE))
TARBALL_DST = $(DL_DIR)/$(HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL) 

define HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_FIXUP
	if ! [ -e $(TARBALL_DST) ] || [ $(TARBALL_SRC) -nt $(TARBALL_DST) ]; then \
		mv -v $(TARBALL_SRC) $(TARBALL_DST); \
	fi
	$(eval HOST_LLVM_OBFUSCATOR_SOURCE=$(HOST_LLVM_OBFUSCATOR_ACTUAL_SOURCE_TARBALL))
endef

HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_HOOKS += HOST_LLVM_OBFUSCATOR_PRE_EXTRACT_FIXUP
SYSROOT = $(shell find $(HOST_DIR) -name 'sysroot' | grep -v llvm-obf)

define HOST_LLVM_OBFUSCATOR_INSTALL_CMDS

	mkdir -p $(HOST_DIR)/opt/llvm-obfuscator
	rsync -az $(@D)/opt/llvm-obfuscator/ $(HOST_DIR)/opt/llvm-obfuscator/
	rsync -az --ignore-existing \
		$(SYSROOT)/lib/ $(HOST_DIR)/opt/llvm-obfuscator/sysroot/lib/
	rsync -az --ignore-existing \
		$(SYSROOT)/usr/lib/ $(HOST_DIR)/opt/llvm-obfuscator/sysroot/usr/lib/
	rsync -az --ignore-existing \
		$(SYSROOT)/usr/include/ $(HOST_DIR)/opt/llvm-obfuscator/sysroot/usr/include/

endef

LLVM_OBF_CC      = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-clang
LLVM_OBF_CXX     = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-clang++
LLVM_OBF_AR      = $(HOST_DIR)/opt/llvm-obfuscator/bin/llvm-ar
LLVM_OBF_STRIP   = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-strip
LLVM_OBF_OBJCOPY = $(HOST_DIR)/opt/llvm-obfuscator/wrappers/bin/arm-linux-gnueabihf-objcopy

LLVM_OBF_HOSTCC  = $(HOST_DIR)/opt/llvm-obfuscator/bin/clang
LLVM_OBF_HOSTCXX = $(HOST_DIR)/opt/llvm-obfuscator/bin/clang++

$(eval $(host-generic-package))
