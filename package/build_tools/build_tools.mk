#############################################################
#
# build_tools
#
#############################################################

BUILD_TOOLS_VERSION = v2.2.4

BUILD_TOOLS_SITE = git@github.com:swift-nav/piksi_build_tools.git
BUILD_TOOLS_SITE_METHOD = git
BUILD_TOOLS_INSTALL_TARGET = YES

define BUILD_TOOLS_BUILD_CMDS
	:
endef

define HOST_BUILD_TOOLS_INSTALL_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/encrypt_and_sign \
		$(HOST_DIR)/usr/bin
endef

$(eval $(host-generic-package))
