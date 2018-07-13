#!/usr/bin/env bash

# Copyright (C) 2016-2018 Swift Navigation Inc.
# Contact: Swift Navigation <dev@swiftnav.com>
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

###### WARNING: FILE AUTOMATICALLY GENERATED, UPDATE M4 FILE TOO #######
###### WARNING: FILE AUTOMATICALLY GENERATED, UPDATE M4 FILE TOO #######
###### WARNING: FILE AUTOMATICALLY GENERATED, UPDATE M4 FILE TOO #######

D=$( (cd "$(dirname "$0")" || exit 1 >/dev/null; pwd -P) )

[[ -z "$DEBUG" ]] || set -x
set -e

if [[ $(uname -a) == *NixOS* ]]; then
  # Remove buildroot from LD_LIBRARY_PATH
  export LD_LIBRARY_PATH=/lib:/usr/lib
fi

FW_VERSION=${1:-v1.5.0-develop-2018071321}
NAP_VERSION=${2:-v1.5.0-develop-2018071321}

FW_S3_PATH=s3://swiftnav-artifacts/piksi_firmware_private/$FW_VERSION/v3
NAP_S3_PATH=s3://swiftnav-artifacts/piksi_fpga/$NAP_VERSION

export AWS_DEFAULT_REGION="us-west-2"

fetch() {
  aws s3 cp "$@"
}

download_fw() {
  FIRMWARE_DIR=firmware/prod

  # Make firmware download dir
  mkdir -p $FIRMWARE_DIR

  # Download piksi_firmware
  fetch $FW_S3_PATH/piksi_firmware_v3_prod.stripped.elf \
    $FIRMWARE_DIR/piksi_firmware.elf

  # Download piksi_fpga, try the prod variant first, then sdk variant
  fetch $NAP_S3_PATH/piksi_prod_fpga.bit $FIRMWARE_DIR/piksi_fpga.bit
}

if [[ -z "$GENERATE_REQUIREMENTS" ]]; then
  download_fw || echo "ERROR: failed to download FPGA and RTOS artifacts"
else
  REQUIREMENTS_M4="$D/requirements.yaml.m4"
  REQUIREMENTS_OUT="${REQUIREMENTS_M4%.m4}"
  [[ -f "$REQUIREMENTS_M4" ]] || { echo "ERROR: could not find $REQUIREMENTS_M4"; exit 1; }
  m4 -DFW_VERSION=$FW_VERSION -DNAP_VERSION=$NAP_VERSION $REQUIREMENTS_M4 >$REQUIREMENTS_OUT
fi