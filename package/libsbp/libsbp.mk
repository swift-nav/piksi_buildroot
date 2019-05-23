################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = woodfell/import_common_cmake_modules
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_GIT_SUBMODULES = YES
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c
LIBSBP_CONF_OPTS = -DSWIFT_PREFERRED_DEPENDENCY_SOURCE=system

$(eval $(cmake-package))
