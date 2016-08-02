################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = v1.1.0
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
