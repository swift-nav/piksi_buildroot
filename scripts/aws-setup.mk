# File: aws-setup.mk

BUILD_VARIANT = $(call qstrip,$(subst piksi_nano,nano,$(subst _defconfig,,$(notdir $(BR2_DEFCONFIG))))
PBR_S3_BUCKET = swiftnav-artifacts

### Macro pbr_s3_url
##  - Arg 1: the prefix/name of the package (e.g. piksi_upgrade_tool)
##  - Arg 2: the version of the package (e.g. v2.2.1)
##  - Arg 3: the name of the asset/file (e.g. piksi_upgrade_tool.tgz)
##
##  Example: 
##    $(call pbr_s3_url,$(UPGRADE_TOOL_PREFIX),$(UPGRADE_TOOL_VERSION),$(UPGRADE_TOOL_ASSET))
##  Produces:
##    s3://swiftnav-artifacts/piksi_upgrade_tool/v2.2.1/piksiv3/piksi_upgrade_tool.tgz
pbr_s3_url = s3://$(PBR_S3_BUCKET)/$1/$2/$(BUILD_VARIANT)/$3

### Macro pbr_s3_src
##  - Arg 1: the prefix/name of the package (e.g. piksi_upgrade_tool)
##  - Arg 2: the version of the package (e.g. v2.2.1)
##  - Arg 3: the name of the asset/file (e.g. piksi_upgrade_tool.tgz)
##
##  Example: 
##    $(call pbr_s3_src,$(UPGRADE_TOOL_PREFIX),$(UPGRADE_TOOL_VERSION),$(UPGRADE_TOOL_ASSET))
##  Produces:
##    s3-piksi_upgrade_tool-v2.2.1-piksi_upgrade_tool.tgz
pbr_s3_src = s3-$(1)-$(2)-$(3)

### Macro pbr_s3_src
##  - Arg 1: the s3 url to copy from (e.g. s3://swiftnav-artifacts/piksi_upgrade_tool/v2.2.1/piksiv3/...)
##  - Arg 2: the download directory, (e.g. <piksi_buildroot>/buildroot/output/dl)
##  - Arg 3: the name of the asset/file (e.g. s3-...-v2.2.1-piksi_upgrade_tool.tgz)
##
##  Example: 
##    $(call pbr_s3_cp,$(UPGRADE_TOOL_PREFIX),$(UPGRADE_TOOL_VERSION),$(UPGRADE_TOOL_ASSET))
##  Produces:
##    aws cp s3://swiftnav-artifacts/piksi_upgrade_tool/v2.2.1/piksiv3/piksi_upgrade_tool.tgz /piksi_buildroot/buildroot/output/dl/s3-piksi_upgrade_tool-v2.2.1-piksi_upgrade_tool.tgz
pbr_s3_cp = aws s3 cp $(1) $(2)/$(3)
