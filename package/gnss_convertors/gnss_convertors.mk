################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = 5383185081534f58ffff620d5fb0c58b889d90a5
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
