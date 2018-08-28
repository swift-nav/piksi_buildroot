################################################################################
#
# metrics_daemon
#
################################################################################

METRICS_DAEMON_VERSION = 0.1
METRICS_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/metrics_daemon"
METRICS_DAEMON_SITE_METHOD = local
METRICS_DAEMON_DEPENDENCIES = json-c libuv nanomsg_custom libsbp libpiksi

define METRICS_DAEMON_USERS
	metricsd -1 metricsd -1 * - - -
endef

define METRICS_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/metrics_daemon $(TARGET_DIR)/usr/bin
endef

define METRICS_DAEMON_BUILD_CMDS
	$(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef

BR2_ROOTFS_OVERLAY += "${METRICS_DAEMON_SITE}/overlay"

$(eval $(generic-package))
