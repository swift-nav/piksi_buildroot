include $(sort $(wildcard $(BR2_EXTERNAL_piksi_buildroot_PATH)/package/*/*.mk))

ifneq ($(BR2_DISABLE_LTO),)

$(info *** Disabling compiler LTO pass... ***)

BR2_TARGET_OPTIMIZATION := $(filter-out,$(BR2_TARGET_OPTIMIZATION),-flto)
BR2_TARGET_CFLAGS := $(filter-out,$(BR2_TARGET_CFLAGS),-flto)
BR2_TARGET_LDFLAGS := $(filter-out,$(BR2_TARGET_LDFLAGS),-flto)

else

# Force -flto into target CFLAGS, flto will get stripped
#   from gcc/ld based on the input (PWD) path.
TARGET_CFLAGS       := $(TARGET_CFLAGS) -flto
TARGET_LDFLAGS      := $(TARGET_LDFLAGS) -flto

LTO_PLUGIN=--plugin $(shell find $(TOOLCHAIN_EXTERNAL_BIN)/.. -name "liblto_plugin.so" | head -1)
$(info LTO_PLUGIN: $(LTO_PLUGIN))

# Don't use LTO for Linux or uboot
NO_FLTO := linux-xilinx,uboot_custom

# The LTO wrapper analyzes the current directory and makes
#   a decision to exclude the -flto parameter.
FLTO_WRAPPER := $(BASE_DIR)/../../scripts/flto-wrapper.c.m4
M4_INC := $(BASE_DIR)/../../scripts

define make-toolchain-wrapper
	$(Q)[ -f $(TOOLCHAIN_EXTERNAL_BIN)/$1.real ] || (     \
	  set -e;                                             \
	  cd $(TOOLCHAIN_EXTERNAL_BIN);                       \
	  cp $1 $1.real;                                      \
	)
	$(Q)[ -f $(TOOLCHAIN_EXTERNAL_BIN)/$1.wrap ] || (     \
	  set -e;                                             \
	  cd $(TOOLCHAIN_EXTERNAL_BIN);                       \
	  wrapper_temp=$$(mktemp);                            \
	  m4 -DM4_TOOL_NAME=$1                                \
	     -DM4_NO_FLTO="$(NO_FLTO)"                        \
	     -DM4_TOOLCHAIN_DIR="$(TOOLCHAIN_EXTERNAL_BIN)"   \
	     $(FLTO_WRAPPER)                                  \
	  | m4 -I $(M4_INC) - >$$wrapper_temp;                \
	  echo "Wrapper temp: $$wrapper_temp";                \
	  $(HOSTCC) -xc - -o $1.wrap <$$wrapper_temp;         \
	  chmod +x $1.wrap                                    \
	)
endef

define toolchain-external-post
	$(info *******************************)
	$(info *** Installing LTO wrappers ***)
	$(info *******************************)
	$(Q)ln -sf $(TOOLCHAIN_EXTERNAL_CROSS)gcc-ar \
	  $(HOST_DIR)/usr/bin/$(TOOLCHAIN_EXTERNAL_PREFIX)-ar
	$(Q)ln -sf $(TOOLCHAIN_EXTERNAL_CROSS)gcc-nm \
	  $(HOST_DIR)/usr/bin/$(TOOLCHAIN_EXTERNAL_PREFIX)-nm
	$(Q)ln -sf $(TOOLCHAIN_EXTERNAL_CROSS)gcc-ranlib \
	  $(HOST_DIR)/usr/bin/$(TOOLCHAIN_EXTERNAL_PREFIX)-ranlib
	$(call make-toolchain-wrapper,$(TOOLCHAIN_EXTERNAL_PREFIX)-gcc)
	$(call make-toolchain-wrapper,$(TOOLCHAIN_EXTERNAL_PREFIX)-ld)
endef

TOOLCHAIN_POST_INSTALL_TARGET_HOOKS += toolchain-external-post

define wrap-toolchain
	( cd $(TOOLCHAIN_EXTERNAL_BIN); \
		cp $(TOOLCHAIN_EXTERNAL_PREFIX)-$1.wrap $(TOOLCHAIN_EXTERNAL_PREFIX)-$1 )
endef

define unwrap-toolchain
	( cd $(TOOLCHAIN_EXTERNAL_BIN); \
		cp $(TOOLCHAIN_EXTERNAL_PREFIX)-$1.real $(TOOLCHAIN_EXTERNAL_PREFIX)-$1 )
endef

define disable-lto-pre
	$(eval TARGET_CFLAGS := $(filter-out -flto,$(TARGET_CFLAGS)))
	$(eval TARGET_LDFLAGS := $(filter-out -flto,$(TARGET_LDFLAGS)))
	$(call wrap-toolchain,gcc)
	$(call wrap-toolchain,ld)
endef

define reenable-lto-post
	$(eval TARGET_CFLAGS := $(TARGET_CFLAGS) -flto)
	$(eval TARGET_LDFLAGS := $(TARGET_LDFLAGS) -flto)
	$(call unwrap-toolchain,gcc)
	$(call unwrap-toolchain,ld)
endef

define uninstall-toolchain-wrapper
	$(Q)[ ! -f $(TOOLCHAIN_EXTERNAL_BIN)/$1.real ] || ( \
		cd $(TOOLCHAIN_EXTERNAL_BIN);                     \
		cp -v $1.real $1;                                 \
		rm -v $1.real;                                    \
	)
	$(Q)[ ! -f $(TOOLCHAIN_EXTERNAL_BIN)/$1.wrap ] || ( \
		cd $(TOOLCHAIN_EXTERNAL_BIN);                     \
		rm -v $1.wrap;                                    \
	)
endef

LINUX_PRE_CONFIGURE_HOOKS += disable-lto-pre
LINUX_POST_INSTALL_HOOKS += reenable-lto-post

UBOOT_CUSTOM_PRE_CONFIGURE_HOOKS += disable-lto-pre
UBOOT_CUSTOM_POST_INSTALL_HOOKS += reenable-lto-post

force-install-toolchain-wrappers:
	$(toolchain-external-post)

force-uninstall-toolchain-wrappers:
	$(call uninstall-toolchain-wrapper,$(TOOLCHAIN_EXTERNAL_PREFIX)-ld)
	$(call uninstall-toolchain-wrapper,$(TOOLCHAIN_EXTERNAL_PREFIX)-gcc)

endif
