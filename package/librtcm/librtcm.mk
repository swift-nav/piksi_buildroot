################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = v0.2.34
LIBRTCM_SITE = https://github.com/swift-nav/librtcm
LIBRTCM_SITE_METHOD = git
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_SUBDIR = c

$(eval $(cmake-package))
