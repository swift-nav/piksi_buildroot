################################################################################
#
# gnss_convertors
#
################################################################################

GNSS_CONVERTORS_VERSION = b20e22ac4d203171978191cf594c7265eb90ac96
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
