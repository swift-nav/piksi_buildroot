#!/bin/sh
set -e

if [ -z "$HW_CONFIG" ]; then
  echo "ERROR: HW_CONFIG is not set"
  exit 1
fi

echo "Installing firmware images for hardware configuration: $HW_CONFIG"

ROOTFS=$1
FIRMWARE_DIR_ROOTFS=$ROOTFS/lib/firmware
FIRMWARE_DIR=$BASE_DIR/../../firmware

VERSION_DIR=$ROOTFS/uimage_ver
mkdir -p $VERSION_DIR
get_git_string_script="$(dirname "$0")"/get_git_string.sh
GIT_STRING=$($get_git_string_script)
TIMESTAMP=$(date +%s)
echo -n $GIT_STRING > $VERSION_DIR/name
echo -n $TIMESTAMP > $VERSION_DIR/timestamp

# Create firmware directory in the rootfs
mkdir -p $FIRMWARE_DIR_ROOTFS

# Remove any existing firmware files
rm -f $FIRMWARE_DIR_ROOTFS/piksi_firmware.elf
rm -f $FIRMWARE_DIR_ROOTFS/piksi_fpga.bit

# Install the piksi_firmware and piksi_fpga images into the rootfs
if [ -e $FIRMWARE_DIR/$HW_CONFIG/piksi_firmware.elf ] && \
   [ -e $FIRMWARE_DIR/$HW_CONFIG/piksi_fpga.bit ]; then
  cp $FIRMWARE_DIR/$HW_CONFIG/piksi_firmware.elf $FIRMWARE_DIR_ROOTFS/piksi_firmware.elf
  cp $FIRMWARE_DIR/$HW_CONFIG/piksi_fpga.bit $FIRMWARE_DIR_ROOTFS/piksi_fpga.bit
else
  echo "*** NO FIRMWARE FILES FOUND, SKIPPING ***"
fi
