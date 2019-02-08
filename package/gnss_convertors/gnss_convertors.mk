################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = bda0e1bf826b11b94bb70bb6706bac48b49b35e8
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
