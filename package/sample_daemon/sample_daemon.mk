################################################################################
#
# sample_daemon
#
################################################################################

SAMPLE_DAEMON_VERSION = 0.1
SAMPLE_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sample_daemon"
SAMPLE_DAEMON_SITE_METHOD = local
SAMPLE_DAEMON_DEPENDENCIES = czmq libsbp libpiksi libcurl libnetwork

define SAMPLE_DAEMON_BUILD_CMDS
    $(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef

define SAMPLE_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/sample_daemon $(TARGET_DIR)/usr/bin
    $(INSTALL) -D -m 0644 $(@D)/sample_daemon.monitrc $(TARGET_DIR)/etc/monitrc.d
    $(INSTALL) -D -m 0755 $(@D)/S83sample_daemon $(TARGET_DIR)/etc/init.d
endef

$(eval $(generic-package))
