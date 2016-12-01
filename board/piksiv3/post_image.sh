#!/bin/sh

UBOOT_DIR=`find $BUILD_DIR -maxdepth 1 -type d -name uboot_custom-*`

generate_dev() {
  local HW=$1
  local CFG=piksiv3_$HW
  local UBOOT_BUILD_DIR=$UBOOT_DIR/build/${CFG}_dev
  local OUTPUT_DIR=$BINARIES_DIR/${CFG}_dev

  mkdir -p $OUTPUT_DIR
  cp $UBOOT_BUILD_DIR/spl/boot.bin $OUTPUT_DIR
  cp $UBOOT_BUILD_DIR/u-boot.img $OUTPUT_DIR
  cp $BINARIES_DIR/uImage.${CFG} $OUTPUT_DIR
  cp $BINARIES_DIR/rootfs.cpio $OUTPUT_DIR
}

generate_prod() {
  local HW=$1
  local CFG=piksiv3_$HW
  local UBOOT_BUILD_DIR=$UBOOT_DIR/build/${CFG}_prod
  local OUTPUT_DIR=$BINARIES_DIR/${CFG}_prod
  local BR_GIT_VERSION=$(git -C $BR2_EXTERNAL rev-parse --short HEAD)

  mkdir -p $OUTPUT_DIR
  cp $UBOOT_BUILD_DIR/tpl/boot.bin $OUTPUT_DIR
  cp $BINARIES_DIR/rootfs.cpio $OUTPUT_DIR

  $UBOOT_BUILD_DIR/tools/image_table_util                                     \
  --append --print --print-images                                             \
  --out $OUTPUT_DIR/image_set.bin                                             \
  --name "Piksi Buildroot $BR_GIT_VERSION"                                    \
  --timestamp $(date +%s)                                                     \
  --hardware $HW                                                              \
  --image $UBOOT_BUILD_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl      \
  --image $UBOOT_BUILD_DIR/u-boot.img --image-type uboot                      \
  --image $BINARIES_DIR/uImage.$CFG --image-type linux
}

generate_dev $HW_CONFIG
generate_prod $HW_CONFIG

# Images for this HW_CONFIG have been moved into their relavant output folder,
# remove the copies from the root of BINARIES_DIR
rm -f $BINARIES_DIR/uImage.*
rm -f $BINARIES_DIR/rootfs.cpio

