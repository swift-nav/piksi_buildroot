################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = ee4bbe60cd7b223040b52ee5ce35d3dc1a5c4164
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
