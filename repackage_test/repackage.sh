#!/bin/bash

UBOOT_PROD_DIR=/home/axlan/src/piksi_buildroot/buildroot/output/build/uboot_custom-3becf333a0f5754a05902a883a0c3b8325194833/build/piksiv3_prod_prod
UBOOT_FAILSAFE_DIR=/home/axlan/src/piksi_buildroot/buildroot/output/build/uboot_custom-3becf333a0f5754a05902a883a0c3b8325194833/build/piksiv3_prod_failsafe
UBOOT_DEV_DIR=/home/axlan/src/piksi_buildroot/buildroot/output/build/uboot_custom-3becf333a0f5754a05902a883a0c3b8325194833/build/piksiv3_prod_dev

FPGA_BITSTREAM=/home/axlan/src/piksi_buildroot/buildroot/output/target/lib/firmware/piksi_fpga.bit
LINUX_IMAGE=/home/axlan/src/piksi_buildroot/buildroot/output/images/uImage.piksiv3_prod

OUTPUT_DIR=/home/axlan/src/piksi_buildroot/repackage_test/out

LOCAL_BUILD=/home/axlan/src/u-boot-xlnx

FAIL_NAME="PiksiMulti-FAILSAFE-jdiamond.bin"
DEV_NAME="PiksiMulti-DEV-jdiamond.bin"
PROD_NAME="PiksiMulti-jdiamond.bin"

FAIL_USE=$UBOOT_FAILSAFE_DIR
DEV_USE=$LOCAL_BUILD
PROD_USE=$UBOOT_PROD_DIR

PROV_IMAGE=$PROD_NAME

echo "Generating FAILSAFE firmware image image..."
$UBOOT_FAILSAFE_DIR/tools/image_table_util --append --print --print-images --out $OUTPUT_DIR/$FAIL_NAME --name 'FSF v1.3.0-develop-2018030818-1-g5090e90-dirty' --timestamp 1520895925 --hardware v3_prod --image $FAIL_USE/spl/u-boot-spl-dtb.img --image-type uboot-spl --image $FAIL_USE/u-boot.img --image-type uboot

# echo "Generating DEV firmware image..."
$UBOOT_DEV_DIR/tools/mkimage -A arm -T firmware -C none -O u-boot -n 'FPGA bitstream' -d $FPGA_BITSTREAM $OUTPUT_DIR/piksi_fpga.img
$UBOOT_DEV_DIR/tools/image_table_util --append --print --print-images --out $OUTPUT_DIR/$DEV_NAME --name 'DEV v1.3.0-develop-2018030818-1-g5090e90-dirty' --timestamp 1520895925 --hardware v3_prod --image $DEV_USE/spl/u-boot-spl-dtb.img --image-type uboot-spl --image $DEV_USE/u-boot.img --image-type uboot --image $OUTPUT_DIR/piksi_fpga.img --image-type fpga

# echo "Generating PROD firmware image image..."
$UBOOT_PROD_DIR/tools/image_table_util --append --print --print-images --out $OUTPUT_DIR/$PROD_NAME --name v1.3.0-develop-2018030818-1-g5090e90-dirty --timestamp 1520895925 --hardware v3_prod --image $PROD_USE/spl/u-boot-spl-dtb.img --image-type uboot-spl --image $PROD_USE/u-boot.img --image-type uboot --image $LINUX_IMAGE --image-type linux

#scp out/$PROV_IMAGE root@192.168.0.222:~
#ssh root@192.168.0.222 upgrade_tool --debug $PROV_IMAGE
