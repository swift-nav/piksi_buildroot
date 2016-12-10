#!/bin/sh

UBOOT_DIR=`find $BUILD_DIR -maxdepth 1 -type d -name uboot_custom-*`
FIRMWARE_DIR=$TARGET_DIR/lib/firmware
BR_GIT_VERSION=$(git -C $BR2_EXTERNAL describe --tags --dirty                 \
                     --always --match 'v[0-9]\.[0-9]')
# remove the githash from the filename for useability (leave it in the header)
FILE_GIT_VERSION=${BR_GIT_VERSION%%-g*}

generate_dev() {
  local HW=$1
  local CFG=piksiv3_$HW
  local UBOOT_BUILD_DIR=$UBOOT_DIR/build/${CFG}_dev
  local OUTPUT_DIR=$BINARIES_DIR/${CFG}_dev

  mkdir -p $OUTPUT_DIR
  cp $BINARIES_DIR/uImage.${CFG} $OUTPUT_DIR

  if [ -e $FIRMWARE_DIR/piksi_fpga.bit ]; then
    $UBOOT_BUILD_DIR/tools/mkimage                                            \
    -A arm -T firmware -C none -O u-boot -n "FPGA bitstream"                  \
    -d $FIRMWARE_DIR/piksi_fpga.bit                                           \
    $OUTPUT_DIR/piksi_fpga.img

    $UBOOT_BUILD_DIR/tools/image_table_util                                   \
    --append --print --print-images                                           \
    --out $OUTPUT_DIR/PiksiMulti-DEV-$FILE_GIT_VERSION.bin                    \
    --name "Piksi Buildroot DEV $BR_GIT_VERSION"                              \
    --timestamp $(date +%s)                                                   \
    --hardware v3_$HW                                                         \
    --image $UBOOT_BUILD_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl    \
    --image $UBOOT_BUILD_DIR/u-boot.img --image-type uboot                    \
    --image $OUTPUT_DIR/piksi_fpga.img --image-type fpga
  fi

  rm $OUTPUT_DIR/piksi_fpga.img
}

generate_prod() {
  local HW=$1
  local CFG=piksiv3_$HW
  local UBOOT_BUILD_DIR=$UBOOT_DIR/build/${CFG}_prod
  local OUTPUT_DIR=$BINARIES_DIR/${CFG}_prod

  mkdir -p $OUTPUT_DIR
  cp $UBOOT_BUILD_DIR/tpl/boot.bin $OUTPUT_DIR

  $UBOOT_BUILD_DIR/tools/image_table_util                                     \
  --append --print --print-images                                             \
  --out $OUTPUT_DIR/PiksiMulti-$FILE_GIT_VERSION.bin                          \
  --name "Piksi Buildroot $BR_GIT_VERSION"                                    \
  --timestamp $(date +%s)                                                     \
  --hardware v3_$HW                                                           \
  --image $UBOOT_BUILD_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl      \
  --image $UBOOT_BUILD_DIR/u-boot.img --image-type uboot                      \
  --image $BINARIES_DIR/uImage.$CFG --image-type linux
}

generate_dev $HW_CONFIG

if [ -e $FIRMWARE_DIR/piksi_firmware.elf ] && \
   [ -e $FIRMWARE_DIR/piksi_fpga.bit ]; then
  generate_prod $HW_CONFIG
else
  echo "*** NO FIRMWARE FILES FOUND, NOT BUILDING PRODUCTION IMAGE ***"
fi

