################################################################################
#
# image_table_tool
#
################################################################################

IMAGE_TABLE_TOOL_VERSION = 0.1
IMAGE_TABLE_TOOL_SITE = "${BR2_EXTERNAL}/package/image_table_tool/src"
IMAGE_TABLE_TOOL_SITE_METHOD = local
IMAGE_TABLE_TOOL_DEPENDENCIES = uboot_custom zlib

IMAGE_TABLE_TOOL_UBOOT_DIR = \
	$(shell find $(BUILD_DIR) -maxdepth 1 -type d -name uboot_custom-*)

define IMAGE_TABLE_TOOL_BUILD_CMDS
	# copy source files from uboot folder
	mkdir -p $(@D)/uboot
	cp $(IMAGE_TABLE_TOOL_UBOOT_DIR)/include/image_table.h $(@D)/uboot
	cp $(IMAGE_TABLE_TOOL_UBOOT_DIR)/common/image_table.c $(@D)/uboot

	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define IMAGE_TABLE_TOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/image_table_tool $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
