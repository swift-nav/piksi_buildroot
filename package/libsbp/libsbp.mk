################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = d2dedfcbf9fb0ae1484102cb5fa1b21df375a881
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
