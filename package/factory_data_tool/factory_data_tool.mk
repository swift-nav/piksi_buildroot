################################################################################
#
# factory_data_tool
#
################################################################################

FACTORY_DATA_TOOL_VERSION = 0.1
FACTORY_DATA_TOOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/factory_data_tool/src"
FACTORY_DATA_TOOL_SITE_METHOD = local
FACTORY_DATA_TOOL_DEPENDENCIES = uboot_custom zlib

FACTORY_DATA_TOOL_UBOOT_DIR = \
	$(shell find $(BUILD_DIR) -maxdepth 1 -type d -name uboot_custom-*)

define FACTORY_DATA_TOOL_BUILD_CMDS
	# copy source files from uboot folder
	mkdir -p $(@D)/uboot
	cp $(FACTORY_DATA_TOOL_UBOOT_DIR)/include/image_table.h $(@D)/uboot
	cp $(FACTORY_DATA_TOOL_UBOOT_DIR)/include/factory_data.h $(@D)/uboot
	cp $(FACTORY_DATA_TOOL_UBOOT_DIR)/common/factory_data.c $(@D)/uboot

	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define FACTORY_DATA_TOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/factory_data_tool $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
