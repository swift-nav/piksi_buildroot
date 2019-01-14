################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = 84520335c95cffe0b1bcb300125b8bcdbca47650
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
