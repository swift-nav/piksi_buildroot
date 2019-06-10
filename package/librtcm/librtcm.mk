################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = v0.2.41
LIBRTCM_SITE = https://github.com/swift-nav/librtcm
LIBRTCM_SITE_METHOD = git
LIBRTCM_SUBDIR = c
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_MAKE_ENV = CFLAGS=-ggdb3

$(eval $(cmake-package))
