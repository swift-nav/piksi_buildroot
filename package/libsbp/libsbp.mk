################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = woodfell/standardise_cmake_dependencies
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
