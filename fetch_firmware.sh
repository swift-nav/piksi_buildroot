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

FW_VERSION=${1:-v1.2.14-4-g811abf13}
NAP_VERSION=${2:-v1.2.14}

FW_S3_PATH=s3://swiftnav-releases/piksi_firmware_private/$FW_VERSION/v3
NAP_S3_PATH=s3://swiftnav-releases/piksi_fpga/$NAP_VERSION
export AWS_DEFAULT_REGION="us-west-2"

fetch() {
  aws s3 cp --no-sign-request "$@" || aws s3 cp "$@"
}

download_fw() {
  HW_CONFIG=$1
  FIRMWARE_DIR=firmware/$HW_CONFIG

  # Make firmware download dir
  mkdir -p $FIRMWARE_DIR

  # Download piksi_firmware
  wget -O $FIRMWARE_DIR/piksi_firmware.elf "https://swiftnav-artifacts-pull-requests.s3.amazonaws.com/piksi_firmware_private/v1.2.14-7-g68f36516/v3/piksi_firmware_v3_prod.stripped.elf?Signature=EWqUQ9AbH18T%2FZqv2FWygL55cQY%3D&Expires=1544601796&AWSAccessKeyId=AKIAI3BP7FDHCVOX4TZA"

  # Download piksi_fpga
  if [ "$HW_CONFIG" == "microzed" ]; then
    # Microzed FPGA image breaks the naming convention so deal with it as a special case
    fetch $NAP_S3_PATH/piksi_microzed_nt1065_fpga.bit $FIRMWARE_DIR/piksi_fpga.bit
  else
    fetch $NAP_S3_PATH/piksi_${HW_CONFIG}_fpga.bit $FIRMWARE_DIR/piksi_fpga.bit
  fi

}

download_fw "prod"
