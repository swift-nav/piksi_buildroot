################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = 402ddc66c5400dbc15dfed2e0686fd09635f3102
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
