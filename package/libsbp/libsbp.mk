################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = SBP_Inertial
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
