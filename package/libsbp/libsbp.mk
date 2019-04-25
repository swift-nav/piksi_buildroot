################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = fa37aa69be28f2b8df26288600decf96c980aff5
LIBSBP_SITE = https://github.com/swift-nav/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
