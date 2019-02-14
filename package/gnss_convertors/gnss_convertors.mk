################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = a77f05bec5f42e691eb1bddf74cdfcc47baae26e
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
