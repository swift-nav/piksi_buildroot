################################################################################
#
# upgrade_tool
#
################################################################################

UPGRADE_TOOL_SOURCE = $(call pbr_s3_src,piksi_upgrade_tool)
UPGRADE_TOOL_SITE = $(DL_DIR)
UPGRADE_TOOL_SITE_METHOD = file

UPGRADE_TOOL_INSTALL_STAGING = YES

define UPGRADE_TOOL_PRE_EXTRACT_FIXUP
	@echo UPGRADE_TOOL_SOURCE: $(UPGRADE_TOOL_SOURCE)
endef

UPGRADE_TOOL_PRE_EXTRACT_HOOKS += UPGRADE_TOOL_PRE_EXTRACT_FIXUP

define UPGRADE_TOOL_INSTALL_TARGET_CMDS
	$(MAKE) TARGET_DIR=$(TARGET_DIR) INSTALL=$(INSTALL) \
		-C $(@D) install
endef

$(eval $(generic-package))
