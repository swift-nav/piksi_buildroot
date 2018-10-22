################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = b3ed01d775c859d9755b0f0c904557567d1f2405
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
