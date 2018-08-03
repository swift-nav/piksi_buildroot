################################################################################
#
# uboot_custom
#
################################################################################

UBOOT_CUSTOM_VERSION = 69b1326504844096af1988a1d6f1d3dd35185e70
UBOOT_CUSTOM_SITE = https://github.com/swift-nav/u-boot-xlnx.git
UBOOT_CUSTOM_SITE_METHOD = git
UBOOT_CUSTOM_DEPENDENCIES = host-dtc host-uboot-tools host-openssl

UBOOT_CUSTOM_RELEASE = $(shell git rev-parse --short $(UBOOT_CUSTOM_VERSION))
UBOOT_CUSTOM_ARCH = $(KERNEL_ARCH)
UBOOT_CUSTOM_MAKE_OPTS += \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	ARCH=$(UBOOT_CUSTOM_ARCH) \
	HOSTCC="$(HOSTCC) $(HOST_CFLAGS)" \
	HOSTLDFLAGS="$(HOST_LDFLAGS)" \
	UBOOTRELEASE="$(UBOOT_CUSTOM_RELEASE)"

define UBOOT_CUSTOM_BUILD_CMDS
	$(foreach cfg,$(call qstrip,$(BR2_PACKAGE_UBOOT_CUSTOM_CONFIGS)), \
		$(TARGET_CONFIGURE_OPTS) \
			$(MAKE) O=$(@D)/build/$(cfg) -C $(@D) $(UBOOT_CUSTOM_MAKE_OPTS) \
				$(cfg)_config; \
		$(TARGET_CONFIGURE_OPTS) \
			$(MAKE) O=$(@D)/build/$(cfg) -C $(@D) $(UBOOT_CUSTOM_MAKE_OPTS); \
	)
endef

$(eval $(generic-package))
