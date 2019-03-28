################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = 4370ae9119182af0f373e65caf51bda654b1a53c
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
