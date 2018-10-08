################################################################################
#
# librtcm
#
################################################################################

LIBRTCM_VERSION = 1062db738226188729209d3e024acd3805a23b17
LIBRTCM_SITE = https://github.com/swift-nav/librtcm
LIBRTCM_SITE_METHOD = git
LIBRTCM_INSTALL_STAGING = YES
LIBRTCM_SUBDIR = c

$(eval $(cmake-package))
