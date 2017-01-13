#!/bin/bash

# Copyright (C) 2016 Swift Navigation Inc.
# Contact: Fergus Noble <fergus@swiftnav.com>
#
# This source is subject to the license found in the file 'LICENSE' which must
# be be distributed together with this source. All other rights reserved.
#
# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
# EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
#
# Script for downloading firmware and NAP binaries from S3 to be incorporated
# into the Linux image.

set -xe

FW_VERSION=${1:-v1.0.1}
NAP_VERSION=${2:-v1.0.1}

FW_S3_PATH=s3://swiftnav-artifacts/piksi_firmware_private/$FW_VERSION/v3
NAP_S3_PATH=s3://swiftnav-artifacts/piksi_fpga/$NAP_VERSION
export AWS_DEFAULT_REGION="us-west-2"

download_fw() {
  HW_CONFIG=$1
  FIRMWARE_DIR=firmware/$HW_CONFIG

  # Make firmware download dir
  mkdir -p $FIRMWARE_DIR

  # Download piksi_firmware
  aws s3 cp $FW_S3_PATH/piksi_firmware_v3_$HW_CONFIG.stripped.elf \
    $FIRMWARE_DIR/piksi_firmware.elf

  # Download piksi_fpga
  if [ "$HW_CONFIG" == "microzed" ]; then
    # Microzed FPGA image breaks the naming convention so deal with it as a special case
    aws s3 cp $NAP_S3_PATH/piksi_microzed_nt1065_fpga.bit $FIRMWARE_DIR/piksi_fpga.bit
  else
    aws s3 cp $NAP_S3_PATH/piksi_${HW_CONFIG}_fpga.bit $FIRMWARE_DIR/piksi_fpga.bit
  fi

}

download_fw "prod"
download_fw "microzed"