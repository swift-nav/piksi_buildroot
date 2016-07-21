################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = 8fdb21382ade3a0be18c87ceebb1f5d8381bdb73
LIBSBP_SITE = $(call github,swift-nav,libsbp,$(FOO_VERSION))
LIBSBP_LICENSE = LGPLv3
LIBSBP_LICENSE_FILES = LICENSE
LIBSBP_INSTALL_STAGING = YES
LIBSBP_DEPENDENCIES = host-pkgconf

$(eval $(cmake-package))
