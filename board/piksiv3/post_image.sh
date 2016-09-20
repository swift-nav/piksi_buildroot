#!/bin/sh

UBOOT_DIR=`find $BUILD_DIR -maxdepth 1 -type d -name uboot_custom-*`

generate_dev() {
  local CFG=$1
  local UBOOT_BUILD_DIR=$UBOOT_DIR/build/${CFG}_dev
  local OUTPUT_DIR=$BINARIES_DIR/${CFG}_dev

  mkdir -p $OUTPUT_DIR
  cp $UBOOT_BUILD_DIR/spl/boot.bin $OUTPUT_DIR
  cp $UBOOT_BUILD_DIR/u-boot.img $OUTPUT_DIR
  cp $BINARIES_DIR/uImage.${CFG} $OUTPUT_DIR
}

generate_prod() {
  local CFG=$1
  local UBOOT_BUILD_DIR=$UBOOT_DIR/build/${CFG}_prod
  local OUTPUT_DIR=$BINARIES_DIR/${CFG}_prod

  mkdir -p $OUTPUT_DIR
  cp $UBOOT_BUILD_DIR/tpl/boot.bin $OUTPUT_DIR

  $UBOOT_BUILD_DIR/tools/image_table_util                                     \
  --append --print --print-images                                             \
  --out $OUTPUT_DIR/image_set.bin                                             \
  --image $UBOOT_BUILD_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl      \
  --image $UBOOT_BUILD_DIR/u-boot.img --image-type uboot                      \
  --image $BINARIES_DIR/uImage.$CFG --image-type linux
}

generate_dev "piksiv3_microzed"
generate_prod "piksiv3_microzed"
