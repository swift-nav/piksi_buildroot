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

define PIKSI_APPS_USERS
	celld -1 celld -1 * - - -
	healthd -1 healthd -1 * - - -
	metricsd -1 metricsd -1 * - - -
	networkd -1 networkd -1 * - - -
	nmead -1 nmead -1 * - - -
	ntripd -1 ntripd -1 * - - -
	oriond -1 oriond -1 * - - -
	otad -1 otad -1 * - - -
	ledd -1 ledd -1 * - - -
	piksi_sys -1 piksi_sys -1 * - - -
	portsd -1 portsd -1 * - - -
	resmond -1 resmond -1 * - - -
	sampld -1 sampld -1 * - - -
	fileio -1 fileio -1 * - - -
	br_nmea -1 br_nmea -1 * - - -
	br_rtcm3 -1 br_rtcm3 -1 * - - -
	configd -1 fileio -1 * - - -
	skylarkd -1 skylarkd -1 * - - -
	stndfl -1 stndfl -1 * - - -
endef

$(eval $(cmake-package))
