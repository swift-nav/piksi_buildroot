################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = 42dbbf0b12725c56abc7e1aeb7d9cf8c5a78a846
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
