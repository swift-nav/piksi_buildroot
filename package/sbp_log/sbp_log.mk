################################################################################
#
# sbp_log
#
################################################################################

SBP_LOG_VERSION = 0.1
SBP_LOG_SITE = "${BR2_EXTERNAL}/package/sbp_log/src"
SBP_LOG_SITE_METHOD = local
SBP_LOG_DEPENDENCIES = czmq libsbp

define SBP_LOG_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_LOG_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/sbp_log $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
