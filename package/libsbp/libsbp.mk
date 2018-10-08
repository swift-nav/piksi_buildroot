################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = f158f4563fa7e69aa29030890a591ecaa6e2ab3e
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
