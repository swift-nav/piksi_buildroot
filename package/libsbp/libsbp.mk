################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = jkretzmer/libsbp/v2.3.4_to_json_c
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
