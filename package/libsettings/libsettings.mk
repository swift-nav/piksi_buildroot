################################################################################
#
# libsettings
#
################################################################################

LIBSETTINGS_VERSION = v0.1.3
LIBSETTINGS_SITE = https://github.com/swift-nav/libsettings
LIBSETTINGS_SITE_METHOD = git
LIBSETTINGS_INSTALL_STAGING = YES
LIBSETTINGS_DEPENDENCIES = libsbp

define LIBSETTINGS_BUILD_CMDS
    $(MAKE) -C $(@D) settings
endef

$(eval $(cmake-package))
