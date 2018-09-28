################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = 0332b5f0030d475541c8d581d153c7b5da37915e
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
