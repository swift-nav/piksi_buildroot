# File: aws-setup.mk

BUILD_VARIANT = $(call qstrip,$(subst _defconfig,,$(notdir $(BR2_DEFCONFIG))))
PBR_S3_BUCKET = swiftnav-artifacts

pbr_s3_url := s3://swiftnav-artifacts/$(PBR_S3_BUCKET)/$1/$(BUILD_VARIANT)/$2
pbr_s3_src := s3-$(1)-$(2)
pbr_s3_cp := aws s3 cp $(1) $(2)/$(3)
