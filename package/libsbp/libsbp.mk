################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = v2.4.2
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
