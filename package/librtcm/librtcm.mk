################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = b940e03d7e8634f285acafd1913428660b8f04dc
LIBRTCM_SITE = https://github.com/swift-nav/librtcm
LIBRTCM_SITE_METHOD = git
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_SUBDIR = c

$(eval $(cmake-package))
