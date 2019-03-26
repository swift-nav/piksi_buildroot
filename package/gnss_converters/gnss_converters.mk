################################################################################
#
# gnss_converters
#
################################################################################

GNSS_CONVERTERS_VERSION = woodfell/standardise_cmake_dependencies
GNSS_CONVERTERS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTERS_SITE_METHOD = git
GNSS_CONVERTERS_INSTALL_STAGING = YES
GNSS_CONVERTERS_SUBDIR = c
GNSS_CONVERTERS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
