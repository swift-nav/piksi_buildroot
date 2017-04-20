################################################################################
#
# upgrade_tool
#
################################################################################

UPGRADE_TOOL_VERSION = 0.1
UPGRADE_TOOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/upgrade_tool/src"
UPGRADE_TOOL_SITE_METHOD = local
UPGRADE_TOOL_DEPENDENCIES = uboot_custom zlib

UPGRADE_TOOL_UBOOT_DIR = \
	$(shell find $(BUILD_DIR) -maxdepth 1 -type d -name uboot_custom-*)

define UPGRADE_TOOL_BUILD_CMDS
	# copy source files from uboot folder
	mkdir -p $(@D)/uboot
	cp $(UPGRADE_TOOL_UBOOT_DIR)/include/image_table.h $(@D)/uboot
	cp $(UPGRADE_TOOL_UBOOT_DIR)/include/factory_data.h $(@D)/uboot
	cp $(UPGRADE_TOOL_UBOOT_DIR)/common/image_table.c $(@D)/uboot
	cp $(UPGRADE_TOOL_UBOOT_DIR)/common/factory_data.c $(@D)/uboot

	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define UPGRADE_TOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/upgrade_tool $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
