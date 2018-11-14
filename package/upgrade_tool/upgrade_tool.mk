################################################################################
#
# upgrade_tool
#
################################################################################

UPGRADE_TOOL_VERSION = v2.2.6
UPGRADE_TOOL_PREFIX = piksi_upgrade_tool
UPGRADE_TOOL_ASSET = piksi_upgrade_tool.tgz
UPGRADE_TOOL_S3 = $(call pbr_s3_url,$(UPGRADE_TOOL_PREFIX),$(UPGRADE_TOOL_VERSION),$(UPGRADE_TOOL_ASSET))
UPGRADE_TOOL_SOURCE = $(call pbr_s3_src,$(UPGRADE_TOOL_PREFIX),$(UPGRADE_TOOL_VERSION),$(UPGRADE_TOOL_ASSET))

UPGRADE_TOOL_SITE = $(DL_DIR)
UPGRADE_TOOL_SITE_METHOD = file

define UPGRADE_TOOL_PRE_DOWNLOAD_FIXUP
	$(call pbr_s3_cp,$(UPGRADE_TOOL_S3),$(UPGRADE_TOOL_SITE),$(UPGRADE_TOOL_SOURCE))
endef

UPGRADE_TOOL_PRE_DOWNLOAD_HOOKS += UPGRADE_TOOL_PRE_DOWNLOAD_FIXUP

define UPGRADE_TOOL_INSTALL_TARGET_CMDS
	$(MAKE) TARGET_DIR=$(TARGET_DIR) INSTALL=$(INSTALL) \
		-C $(@D) install
endef

$(eval $(generic-package))
