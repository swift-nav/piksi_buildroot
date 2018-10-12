################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = silverjam/resource_monitor
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
#LIBSBP_SITE = /host/home/dev/libsbp
#LIBSBP_SITE_METHOD = local
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
