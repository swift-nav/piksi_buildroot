################################################################################
#
# upgrade_tool
#
################################################################################

UPGRADE_TOOL_VERSION = v1.5.0-develop-2018091223
UPGRADE_TOOL_SOURCE = piksi_upgrade_tool.tgz
UPGRADE_TOOL_SITE = https://github.com/swift-nav/piksi_upgrade_tool_bin/releases/download/$(UPGRADE_TOOL_VERSION)

define UPGRADE_TOOL_INSTALL_TARGET_CMDS
	$(MAKE) TARGET_DIR=$(TARGET_DIR) INSTALL=$(INSTALL) \
		-C $(@D) install
endef

$(eval $(generic-package))
