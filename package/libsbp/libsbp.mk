################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = fdb525da507250981a5e1e1d6aed753dbc9f4120
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
