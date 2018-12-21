################################################################################
#
# libsettings
#
################################################################################

LIBSETTINGS_VERSION = v0.1.1
LIBSETTINGS_SITE = https://github.com/swift-nav/libsettings
LIBSETTINGS_SITE_METHOD = git
LIBSETTINGS_INSTALL_STAGING = YES
LIBSETTINGS_DEPENDENCIES = libsbp

$(eval $(cmake-package))
