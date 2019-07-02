################################################################################
#
# gnss_convertors
#
################################################################################

# Workaround a Jenkins issue, this is the hash for v0.3.85 but Jenkins
#   has cached v0.3.85 incorrectly.
GNSS_CONVERTORS_VERSION = v0.3.86
GNSS_CONVERTORS_SITE = git@github.com:swift-nav/gnss-converters.git
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_GIT_SUBMODULES = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_MAKE_ENV = CFLAGS=-ggdb3

$(eval $(cmake-package))
