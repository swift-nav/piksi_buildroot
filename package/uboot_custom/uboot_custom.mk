################################################################################
#
# uboot_custom
#
################################################################################

UBOOT_CUSTOM_VERSION = 00ae34f6dabadea8c383388526ad8c78f61d1122
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

define UBOOT_CUSTOM_POST_INSTALL
	@echo '>>>' uboot_custom post install...
	@if [ "$(VARIANT)" == "release" ]; then \
		echo '>>>' uboot_custom post install: release image, running custom steps...; \
		HW_CONFIG=$(HW_CONFIG) \
		BINARIES_DIR=$(BINARIES_DIR) \
		TARGET_DIR=$(TARGET_DIR) \
		BUILD_DIR=$(BUILD_DIR) \
		BR2_JUST_GEN_FAILSAFE=y \
			"${BR2_EXTERNAL_piksi_buildroot_PATH}/board/piksiv3/post_image.sh"; \
	else \
		echo '>>>' uboot_custom post install: not a release image, no custom steps...; \
	fi
endef

UBOOT_CUSTOM_POST_INSTALL_TARGET_HOOKS += UBOOT_CUSTOM_POST_INSTALL

$(eval $(generic-package))
