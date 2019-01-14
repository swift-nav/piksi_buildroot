################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = 9a537c1c245ab7917f8d8e2cb5d8ad776eeb3151
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
