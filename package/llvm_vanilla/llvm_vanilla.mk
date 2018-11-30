################################################################################
#
# llvm_vanilla
#
################################################################################

HOST_LLVM_VANILLA_VERSION = v18
HOST_LLVM_VANILLA_SOURCE = $(HOST_LLVM_VANILLA_VERSION)/llvm-vanilla-arm-x86.txz
HOST_LLVM_VANILLA_SITE = https://github.com/swift-nav/llvm-obfuscator-arm/releases/download
HOST_LLVM_VANILLA_ACTUAL_SOURCE_TARBALL = llvm-vanilla-$(HOST_LLVM_VANILLA_VERSION).tar.xz

LLVM_VNL_TARBALL_SRC = $(DL_DIR)/$(shell basename $(HOST_LLVM_VANILLA_SOURCE))
LLVM_VNL_TARBALL_DST = $(DL_DIR)/$(HOST_LLVM_VANILLA_ACTUAL_SOURCE_TARBALL) 

## If .tar.xz (DST) does not exist but .txz (SRC) does, then create symlink DST -> SRC
## If DST does exist but SRC is newer, then remove DST and create symlink DST -> SRC
define HOST_LLVM_VANILLA_PRE_EXTRACT_FIXUP
	if ! [ -e $(LLVM_VNL_TARBALL_DST) ] && [ -e $(LLVM_VNL_TARBALL_SRC) ]; then \
	    ln -sf $(LLVM_VNL_TARBALL_SRC) $(LLVM_VNL_TARBALL_DST); \
	else \
		if [ -e $(LLVM_VNL_TARBALL_DST) ] && [ $(LLVM_VNL_TARBALL_SRC) -nt $(LLVM_VNL_TARBALL_DST) ]; then \
			rm -rf $(LLVM_VNL_TARBALL_DST); \
			ln -sf $(LLVM_VNL_TARBALL_SRC) $(LLVM_VNL_TARBALL_DST); \
		fi \
	fi \
	$(eval HOST_LLVM_VANILLA_SOURCE=$(HOST_LLVM_VANILLA_ACTUAL_SOURCE_TARBALL))
endef

HOST_LLVM_VANILLA_PRE_EXTRACT_HOOKS += HOST_LLVM_VANILLA_PRE_EXTRACT_FIXUP
SYSROOT = $(shell find $(HOST_DIR) -name 'sysroot' | grep -v llvm-obf)

define HOST_LLVM_VANILLA_INSTALL_CMDS
	mkdir -p $(HOST_DIR)/opt/llvm-vanilla
	rsync -az $(@D)/opt/llvm-vanilla/ $(HOST_DIR)/opt/llvm-vanilla/
endef

LLVM_CC      = $(HOST_DIR)/opt/llvm-vanilla/wrappers/bin/arm-linux-gnueabihf-clang
LLVM_CXX     = $(HOST_DIR)/opt/llvm-vanilla/wrappers/bin/arm-linux-gnueabihf-clang++
LLVM_AR      = $(HOST_DIR)/opt/llvm-vanilla/bin/llvm-ar
LLVM_STRIP   = $(HOST_DIR)/opt/llvm-vanilla/wrappers/bin/arm-linux-gnueabihf-strip
LLVM_OBJCOPY = $(HOST_DIR)/opt/llvm-vanilla/wrappers/bin/arm-linux-gnueabihf-objcopy

LLVM_HOSTCC  = $(HOST_DIR)/opt/llvm-vanilla/bin/clang
LLVM_HOSTCXX = $(HOST_DIR)/opt/llvm-vanilla/bin/clang++

$(eval $(host-generic-package))

