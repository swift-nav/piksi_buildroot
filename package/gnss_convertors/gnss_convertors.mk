################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = 285dd0102528e0d3fa5d7f4a28dd7484a744d683
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = librtcm libsbp

$(eval $(cmake-package))
