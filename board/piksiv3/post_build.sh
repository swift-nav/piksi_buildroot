#!/bin/sh
set -e

echo "Installing firmware images for hardware configuration: $HW_CONFIG"

ROOTFS=$1
FIRMWARE_DIR_ROOTFS=$ROOTFS/lib/firmware
FIRMWARE_DIR=$BASE_DIR/../../firmware

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

