################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = v0.2.43
LIBRTCM_SITE = git@github.com:swift-nav/librtcm.git
LIBRTCM_SITE_METHOD = git
LIBRTCM_GIT_SUBMODULES = YES
LIBRTCM_SUBDIR = c
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_MAKE_ENV = CFLAGS=-ggdb3

$(eval $(cmake-package))
