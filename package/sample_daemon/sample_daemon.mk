################################################################################
#
# sample_daemon
#
################################################################################

SAMPLE_DAEMON_VERSION = 0.1
SAMPLE_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sample_daemon"
SAMPLE_DAEMON_SITE_METHOD = local
SAMPLE_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

ifeq    ($(BR2_BUILD_SAMPLE_DAEMON),y)
define SAMPLE_DAEMON_INVOKE_MAKE
    $(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D)/src all
endef
endif # ($(BR2_BUILD_SAMPLE_DAEMON),y)

define SAMPLE_DAEMON_BUILD_CMDS
	$(SAMPLE_DAEMON_INVOKE_MAKE)
endef

ifeq    ($(BR2_BUILD_SAMPLE_DAEMON),y)
define SAMPLE_DAEMON_INVOKE_INSTALL
    $(INSTALL) -D -m 0755 $(@D)/src/sample_daemon $(TARGET_DIR)/usr/bin
    $(INSTALL) -D -m 0644 $(@D)/sample_daemon.monitrc $(TARGET_DIR)/etc/monitrc.d
    $(INSTALL) -D -m 0755 $(@D)/S83sample_daemon $(TARGET_DIR)/etc/init.d
endef
endif # ($(BR2_BUILD_SAMPLE_DAEMON),y)

define SAMPLE_DAEMON_INSTALL_TARGET_CMDS
	$(SAMPLE_DAEMON_INVOKE_INSTALL)
endef

$(eval $(generic-package))
