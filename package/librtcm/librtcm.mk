################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = 580af1909227e6751ed40192626f0f3e59155660
LIBRTCM_SITE = https://github.com/swift-nav/librtcm
LIBRTCM_SITE_METHOD = git
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_SUBDIR = c

$(eval $(cmake-package))
