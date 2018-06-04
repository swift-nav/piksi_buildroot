################################################################################
#
# sbp_cli
#
################################################################################

SBP_CLI_VERSION = 0.1
SBP_CLI_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_cli/src"
SBP_CLI_SITE_METHOD = local
SBP_CLI_DEPENDENCIES = nanomsg libsbp

define SBP_CLI_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_CLI_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/sbp_log $(TARGET_DIR)/usr/bin
    $(INSTALL) -D -m 0755 $(@D)/sbp_cmd_resp $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
