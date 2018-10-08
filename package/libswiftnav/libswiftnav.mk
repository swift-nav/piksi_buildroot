################################################################################
#
# libswiftnav
#
################################################################################

LIBSWIFTNAV_VERSION = 4f767ef2490d26fdb38681cb379eb14f10a9a594
LIBSWIFTNAV_SITE = https://github.com/swift-nav/libswiftnav
LIBSWIFTNAV_SITE_METHOD = git
LIBSWIFTNAV_INSTALL_STAGING = YES

$(eval $(cmake-package))
