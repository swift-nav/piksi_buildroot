# File: aws-setup.mk

BUILD_VARIANT = $(call qstrip,$(subst _defconfig,,$(notdir $(BR2_DEFCONFIG))))
PBR_S3_BUCKET = swiftnav-artifacts

### Macro pbr_s3_url
##  - Arg 1: the prefix of the software being downloaded (e.g. piksi_upgrade_tool)
##  - Arg 2: 
##  - Arg 3: 
##
##  Example: 
##    UPGRADE_TOOL_S3 = $(call pbr_s3_url,$(UPGRADE_TOOL_PREFIX),$(UPGRADE_TOOL_VERSION),$(UPGRADE_TOOL_ASSET))
##  Produces:
##    UPGRADE_TOOL_S3 = s3://swiftnav-artifacts/piksi_upgrade_tool/v2.2.1/piksiv3/piksi_upgrade_tool.tgz
pbr_s3_url = s3://$(PBR_S3_BUCKET)/$1/$2/$(BUILD_VARIANT)/$3

pbr_s3_src = s3-$(1)-$(2)-$(3)

pbr_s3_cp = aws s3 cp $(1) $(2)/$(3)
