################################################################################
#
# rpmsg_stats_daemon
#
################################################################################

RPMSG_STATS_DAEMON_VERSION = 0.1
RPMSG_STATS_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/rpmsg_stats_daemon/src"
RPMSG_STATS_DAEMON_SITE_METHOD = local
RPMSG_STATS_DAEMON_DEPENDENCIES = libuv libpiksi libswiftnav
RPMSG_STATS_DAEMON_INSTALL_STAGING = YES

define RPMSG_STATS_DAEMON_USERS
	rpmsg_statsd -1 rpmsg_statsd -1 * - - -
endef

define RPMSG_STATS_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define RPMSG_STATS_DAEMON_INSTALL_STAGING_CMDS
    $(INSTALL) -d -m 0755 $(STAGING_DIR)/usr/include/rpmsg_stats
    $(INSTALL) -D -m 0644 $(@D)/rpmsg_stats.h \
                          $(STAGING_DIR)/usr/include/rpmsg_stats
endef

define RPMSG_STATS_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/rpmsg_stats_daemon $(TARGET_DIR)/usr/bin
endef

BR2_ROOTFS_OVERLAY += "${RPMSG_STATS_DAEMON_SITE}/../overlay"

$(eval $(generic-package))
