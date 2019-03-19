################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = 863a06a69d882f069f0c54a7200e2e393024c51a # silverjam/sbp-nmea-gsv-wrapper
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
