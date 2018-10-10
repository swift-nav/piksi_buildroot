################################################################################
#
# libsettings
#
################################################################################

LIBSETTINGS_VERSION = pmiettinen/devc-1185-libsettings
LIBSETTINGS_SITE = /mnt/users/pasi/swiftnav/libsettings
LIBSETTINGS_SITE_METHOD = local
LIBSETTINGS_INSTALL_STAGING = YES
LIBSETTINGS_DEPENDENCIES = libsbp

$(eval $(cmake-package))
