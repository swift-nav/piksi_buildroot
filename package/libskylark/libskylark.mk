################################################################################
#
# libskylark
#
################################################################################

LIBSKYLARK_VERSION = 0f0a735a2ef5325845c94820b223410fe9673f96
LIBSKYLARK_SITE = $(call github,swift-nav,libskylark,$(FOO_VERSION))
LIBSKYLARK_LICENSE = MIT
LIBSKYLARK_LICENSE_FILES = LICENSE
LIBSKYLARK_INSTALL_STAGING = YES
LIBSKYLARK_DEPENDENCIES = libcurl libserialport host-pkgconf

$(eval $(cmake-package))
