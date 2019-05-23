################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = woodfell/import_common_cmake_modules
LIBRTCM_SITE = https://github.com/swift-nav/librtcm
LIBRTCM_SITE_METHOD = git
LIBRTCM_GIT_SUBMODULES = YES
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_SUBDIR = c
LIBRTCM_CONF_OPTS = -DSWIFT_PREFERRED_DEPENDENCY_SOURCE=system

$(eval $(cmake-package))
