################################################################################
#
# libswiftnav
#
################################################################################

LIBSWIFTNAV_VERSION = v0.1.8
LIBSWIFTNAV_SITE = git@github.com:swift-nav/libswiftnav.git
LIBSWIFTNAV_SITE_METHOD = git
LIBSWIFTNAV_GIT_SUBMODULES = YES
LIBSWIFTNAV_INSTALL_STAGING = YES

$(eval $(cmake-package))
