################################################################################
#
# libswiftnav
#
################################################################################

LIBSKYLARK_VERSION = 459cb3bc0fa1d0d4b054e32e3d14cccce3397e72
NAVENGINE_SITE_METHOD = git
NAVENGINE_SITE = ssh://git@github.com:swift-nav/libswiftnav-private.git
LIBSKYLARK_REDISTRIBUTE = NO
LIBSKYLARK_INSTALL_STAGING = YES
LIBSKYLARK_DEPENDENCIES = host-pkgconf

$(eval $(cmake-package))
