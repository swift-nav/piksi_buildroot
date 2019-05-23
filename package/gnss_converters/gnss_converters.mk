################################################################################
#
# gnss_converters
#
################################################################################

GNSS_CONVERTERS_VERSION = woodfell/import_standard_cmake_modules
GNSS_CONVERTERS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTERS_SITE_METHOD = git
GNSS_CONVERTERS_GIT_SUBMODULES = YES
GNSS_CONVERTERS_INSTALL_STAGING = YES
GNSS_CONVERTERS_SUBDIR = c
GNSS_CONVERTERS_CONF_OPTS = -DSWIFT_PREFERRED_DEPENDENCY_SOURCE=system
GNSS_CONVERTERS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
