################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = v2.6.4
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_GIT_SUBMODULES = YES
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
