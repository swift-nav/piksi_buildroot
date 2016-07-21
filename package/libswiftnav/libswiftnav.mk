################################################################################
#
# libswiftnav
#
################################################################################

LIBSKYLARK_VERSION = 459cb3bc0fa1d0d4b054e32e3d14cccce3397e72
LIBSKYLARK_SITE = $(call github,swift-nav,libswiftnav-private,$(FOO_VERSION))
LIBSKYLARK_REDISTRIBUTE = NO
LIBSKYLARK_INSTALL_STAGING = YES
LIBSKYLARK_DEPENDENCIES = host-pkgconf

$(eval $(cmake-package))
