################################################################################
#
# skylark_download_daemon
#
################################################################################

SKYLARK_DOWNLOAD_DAEMON_VERSION = 0.1
SKYLARK_DOWNLOAD_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/skylark_download_daemon/src"
SKYLARK_DOWNLOAD_DAEMON_SITE_METHOD = local
SKYLARK_DOWNLOAD_DAEMON_DEPENDENCIES = czmq libsbp libpiksi libcurl libnetwork

define SKYLARK_DOWNLOAD_DAEMON_BUILD_CMDS
		CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
    	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SKYLARK_DOWNLOAD_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/skylark_download_daemon $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
