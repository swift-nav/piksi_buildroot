################################################################################
#
# piksi_apps
#
################################################################################

PIKSI_APPS_VERSION = piksi_apps_import
PIKSI_APPS_SITE = git@github.com:woodfell/piksi_apps.git
PIKSI_APPS_SITE_METHOD = git
PIKSI_APPS_INSTALL_STAGING = YES
PIKSI_APPS_DEPENDENCIES = libsbp libsettings cmph gnss_converters libswiftnav json-c libuv libyaml libcurl libsocketcan librtcm

$(eval $(cmake-package))
