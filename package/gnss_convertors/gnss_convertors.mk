################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = b0fc489e8956224906049e9ea7a678e38fb5c55d
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
