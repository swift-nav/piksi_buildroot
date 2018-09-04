################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = 384b93d33e201d6fb5473a34321bbc6b075cb741
LIBRTCM_SITE = https://github.com/swift-nav/librtcm
LIBRTCM_SITE_METHOD = git
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_SUBDIR = c

$(eval $(cmake-package))
