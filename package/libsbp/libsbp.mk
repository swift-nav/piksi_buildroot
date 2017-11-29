################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = cce8a1e370a456af8a056fbfc42e1e35624c4a4c
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
