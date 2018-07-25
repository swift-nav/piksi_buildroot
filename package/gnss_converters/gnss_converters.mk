################################################################################
#
# gnss_converters
#
################################################################################

GNSS_CONVERTERS_VERSION = v0.3.54
GNSS_CONVERTERS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTERS_SITE_METHOD = git
GNSS_CONVERTERS_INSTALL_STAGING = YES
GNSS_CONVERTERS_SUBDIR = c
GNSS_CONVERTERS_DEPENDENCIES = librtcm libsbp

$(eval $(cmake-package))
