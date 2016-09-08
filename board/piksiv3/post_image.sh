#!/bin/sh

UBOOT_DIR=`find $BUILD_DIR -maxdepth 1 -type d -name uboot-*`

cp $UBOOT_DIR/spl/u-boot-spl-dtb.img $BINARIES_DIR
cp $UBOOT_DIR/tpl/boot.bin $BINARIES_DIR

$UBOOT_DIR/tools/image_table_util                                             \
--append --print --print-images                                               \
--out $BINARIES_DIR/image_set.bin                                             \
--image $BINARIES_DIR/u-boot-spl-dtb.img --image-type uboot-spl               \
--image $BINARIES_DIR/u-boot.img --image-type uboot                           \
--image $BINARIES_DIR/uImage.piksiv3_microzed --image-type linux

