################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = v0.3.19
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = librtcm libsbp

$(eval $(cmake-package))
