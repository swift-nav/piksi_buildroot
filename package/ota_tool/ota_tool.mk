################################################################################
#
# ota_tool
#
################################################################################

OTA_TOOL_VERSION = 0.1.0
OTA_TOOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/ota_tool"
OTA_TOOL_SITE_METHOD = local
OTA_TOOL_DEPENDENCIES = host-openssl host-rustc host-cargo

OTA_TOOL_CARGO_ENV = CARGO_HOME=$(HOST_DIR)/share/cargo \
											 OPENSSL_DIR=$(STAGING_DIR)/usr \
											 CC=$(TARGET_CC) \
											 CFLAGS="$(TARGET_CFLAGS)"
OTA_TOOL_CARGO_MODE = release#$(if $(BR2_ENABLE_DEBUG),debug,release)

OTA_TOOL_BIN_DIR = target/$(RUSTC_TARGET_NAME)/$(OTA_TOOL_CARGO_MODE)

OTA_TOOL_CARGO_OPTS = \
  --$(OTA_TOOL_CARGO_MODE) \
	--target=$(RUSTC_TARGET_NAME) \
	--manifest-path=$(@D)/Cargo.toml

define OTA_TOOL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(OTA_TOOL_CARGO_ENV) \
		cargo build $(OTA_TOOL_CARGO_OPTS)
endef

define OTA_TOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/$(OTA_TOOL_BIN_DIR)/ota_tool \
		$(TARGET_DIR)/usr/bin/ota_tool
endef

$(eval $(generic-package))
