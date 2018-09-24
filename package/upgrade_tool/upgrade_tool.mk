################################################################################
#
# upgrade_tool
#
################################################################################

BUILD_VARIANT=$(call qstrip,$(subst _defconfig,,$(notdir $(BR2_DEFCONFIG))))

UPGRADE_TOOL_VERSION = v2.2.1
UPGRADE_TOOL_S3 = s3://swiftnav-artifacts/piksi_upgrade_tool/$(UPGRADE_TOOL_VERSION)/$(BUILD_VARIANT)/piksi_upgrade_tool.tgz
UPGRADE_TOOL_SOURCE = s3-piksi_upgrade_tool-$(UPGRADE_TOOL_VERSION).tgz
UPGRADE_TOOL_SITE = $(DL_DIR)
UPGRADE_TOOL_SITE_METHOD = file

define UPGRADE_TOOL_INSTALL_TARGET_CMDS
	$(MAKE) TARGET_DIR=$(TARGET_DIR) INSTALL=$(INSTALL) \
		-C $(@D) install
endef

define UPGRADE_TOOL_PRE_DOWNLOAD_FIXUP
	aws s3 cp $(UPGRADE_TOOL_S3) $(UPGRADE_TOOL_SITE)/$(UPGRADE_TOOL_SOURCE)
endef

UPGRADE_TOOL_PRE_DOWNLOAD_HOOKS += UPGRADE_TOOL_PRE_DOWNLOAD_FIXUP

$(eval $(generic-package))
