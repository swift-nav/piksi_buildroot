# File: aws-setup.mk

### Macro pbr_s3_src
##  - Arg 1: the prefix/name of the package (e.g. piksi_upgrade_tool)
##
##  Example: 
##    $(call pbr_s3_src,$(UPGRADE_TOOL_PREFIX))
##  Produces:
##    s3-piksi_upgrade_tool-v2.2.1-piksi_upgrade_tool.tgz
pbr_s3_src = $(shell basename $(shell $(BR2_EXTERNAL_piksi_buildroot_PATH)/scripts/get-named-artifact $(VARIANT) $(1) local_path))
