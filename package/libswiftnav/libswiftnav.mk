################################################################################
#
# libswiftnav
#
################################################################################

LIBSWIFTNAV_VERSION = v0.1.8
LIBSWIFTNAV_SITE = https://github.com/swift-nav/libswiftnav
LIBSWIFTNAV_SITE_METHOD = git
LIBSWIFTNAV_GIT_SUBMODULES = YES
LIBSWIFTNAV_INSTALL_STAGING = YES

$(eval $(cmake-package))
