################################################################################
#
# sbp_fileio_daemon
#
################################################################################

SBP_FILEIO_DAEMON_VERSION = 0.1
SBP_FILEIO_DAEMON_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_fileio_daemon/src"
SBP_FILEIO_DAEMON_SITE_METHOD = local
SBP_FILEIO_DAEMON_DEPENDENCIES = czmq libsbp libpiksi

define SBP_FILEIO_DAEMON_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_FILEIO_DAEMON_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/sbp_fileio_daemon $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
