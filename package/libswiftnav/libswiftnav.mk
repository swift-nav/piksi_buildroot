################################################################################
#
# libswiftnav
#
################################################################################

LIBSWIFTNAV_VERSION = v0.1.0
LIBSWIFTNAV_SITE = https://github.com/swift-nav/libswiftnav
LIBSWIFTNAV_SITE_METHOD = git
LIBSWIFTNAV_INSTALL_STAGING = YES

$(eval $(cmake-package))
