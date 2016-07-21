################################################################################
#
# navengine
#
################################################################################

NAVENGINE_VERSION = 459cb3bc0fa1d0d4b054e32e3d14cccce3397e72
NAVENGINE_SITE = $(call github,swift-nav,navengine,$(FOO_VERSION))
NAVENGINE_REDISTRIBUTE = NO
NAVENGINE_DEPENDENCIES = eigen gtest gflags libcurl libsbp libserialport \
                         libswiftnav libskylark host-pkgconf openssl zlib

$(eval $(cmake-package))
