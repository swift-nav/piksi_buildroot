################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = pmiettinen/esd-1012-settings-register-resp
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
