################################################################################
#
# upgrade_tool
#
################################################################################

UPGRADE_TOOL_VERSION = v2.1.6
UPGRADE_TOOL_SITE = $(call github,swift-nav,piksi_upgrade_tool_bin,$(UPGRADE_TOOL_VERSION))

define UPGRADE_TOOL_INSTALL_TARGET_CMDS
	$(MAKE) TARGET_DIR=$(TARGET_DIR) INSTALL=$(INSTALL) \
		-C $(@D) install
endef

$(eval $(generic-package))
