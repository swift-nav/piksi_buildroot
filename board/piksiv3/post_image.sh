#!/bin/sh

CFG=piksiv3_$HW_CONFIG
GIT_STRING=$(git -C $BR2_EXTERNAL describe --tags --dirty                     \
                 --always --long --match 'v[0-9]*\.[0-9]*\.[0-9]*')
# remove the githash from the filename for useability
FILE_GIT_STRING=${GIT_STRING%%-g*}

OUTPUT_DIR=$BINARIES_DIR/${CFG}
FIRMWARE_DIR=$TARGET_DIR/lib/firmware
UBOOT_BASE_DIR=`find $BUILD_DIR -maxdepth 1 -type d -name uboot_custom-*`
UBOOT_PROD_DIR=$UBOOT_BASE_DIR/build/${CFG}_prod
UBOOT_FAILSAFE_DIR=$UBOOT_BASE_DIR/build/${CFG}_failsafe
UBOOT_DEV_DIR=$UBOOT_BASE_DIR/build/${CFG}_dev

generate_dev() {
  $UBOOT_DEV_DIR/tools/mkimage                                                \
  -A arm -T firmware -C none -O u-boot -n "FPGA bitstream"                    \
  -d $FIRMWARE_DIR/piksi_fpga.bit                                             \
  $OUTPUT_DIR/piksi_fpga.img

  $UBOOT_DEV_DIR/tools/image_table_util                                       \
  --append --print --print-images                                             \
  --out "$OUTPUT_DIR/PiksiMulti-DEV-$FILE_GIT_STRING.bin"                     \
  --name "DEV $GIT_STRING"                                                    \
  --timestamp $(date +%s)                                                     \
  --hardware v3_$HW_CONFIG                                                    \
  --image $UBOOT_DEV_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl        \
  --image $UBOOT_DEV_DIR/u-boot.img --image-type uboot                        \
  --image $OUTPUT_DIR/piksi_fpga.img --image-type fpga

  rm $OUTPUT_DIR/piksi_fpga.img
}

generate_prod() {
  $UBOOT_PROD_DIR/tools/image_table_util                                      \
  --append --print --print-images                                             \
  --out "$OUTPUT_DIR/PiksiMulti-$FILE_GIT_STRING.bin"                         \
  --name "$GIT_STRING"                                                        \
  --timestamp $(date +%s)                                                     \
  --hardware v3_$HW_CONFIG                                                    \
  --image $UBOOT_PROD_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl       \
  --image $UBOOT_PROD_DIR/u-boot.img --image-type uboot                       \
  --image $BINARIES_DIR/uImage.$CFG --image-type linux
}

generate_failsafe() {
  $UBOOT_FAILSAFE_DIR/tools/image_table_util                                  \
  --append --print --print-images                                             \
  --out "$OUTPUT_DIR/PiksiMulti-FAILSAFE-$FILE_GIT_STRING.bin"                \
  --name "FSF $GIT_STRING"                                                    \
  --timestamp $(date +%s)                                                     \
  --hardware v3_$HW_CONFIG                                                    \
  --image $UBOOT_FAILSAFE_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl   \
  --image $UBOOT_FAILSAFE_DIR/u-boot.img --image-type uboot
}

mkdir -p $OUTPUT_DIR

cp $UBOOT_PROD_DIR/tpl/boot.bin $OUTPUT_DIR
cp $BINARIES_DIR/uImage.${CFG} $OUTPUT_DIR

generate_failsafe

if [ -e $FIRMWARE_DIR/piksi_fpga.bit ]; then
  generate_dev
else
  echo "*** NO FPGA FIRMWARE FOUND, NOT BUILDING DEVELOPMENT IMAGE ***"
fi

if [ -e $FIRMWARE_DIR/piksi_firmware.elf ] && \
   [ -e $FIRMWARE_DIR/piksi_fpga.bit ]; then
  generate_prod
else
  echo "*** NO FIRMWARE FILES FOUND, NOT BUILDING PRODUCTION IMAGE ***"
fi

