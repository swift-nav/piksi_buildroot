################################################################################
#
# upgrade_tool
#
################################################################################

UPGRADE_TOOL_VERSION = v1.4.0-develop-2018061323
UPGRADE_TOOL_SITE = git@github.com:swift-nav/piksi_upgrade_tool_bin.git
UPGRADE_TOOL_SITE_METHOD = git

define UPGRADE_TOOL_INSTALL_TARGET_CMDS
	$(MAKE) TARGET_DIR=$(TARGET_DIR) INSTALL=$(INSTALL) \
		-C $(@D) install
endef

$(eval $(generic-package))
