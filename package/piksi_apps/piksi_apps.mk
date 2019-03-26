################################################################################
#
# piksi_apps
#
################################################################################

PIKSI_APPS_VERSION = woodfell/piksi_apps
PIKSI_APPS_SITE = https://github.com/swift-nav/piksi_apps.git
PIKSI_APPS_SITE_METHOD = git
PIKSI_APPS_INSTALL_STAGING = YES
PIKSI_APPS_DEPENDENCIES = libsbp libsettings cmph gnss_converters libswiftnav json-c libuv libyaml libcurl libsocketcan librtcm

$(eval $(cmake-package))
