################################################################################
#
# libswiftnav
#
################################################################################

LIBSWIFTNAV_VERSION = woodfell/import_standard_cmake_modules
LIBSWIFTNAV_SITE = https://github.com/swift-nav/libswiftnav
LIBSWIFTNAV_SITE_METHOD = git
LIBSWIFTNAV_GIT_SUBMODULES = YES
LIBSWIFTNAV_INSTALL_STAGING = YES
LIBSWIFTNAV_CONF_OPTS = -DSWIFT_PREFERRED_DEPENDENCY_SOURCE=system

$(eval $(cmake-package))
