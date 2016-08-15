#!/bin/sh

OUTPUT_DIR="build_sd"

mkdir -p "$OUTPUT_DIR"

# Copy files required to boot from an SD card
cp "./uEnv.txt" "$OUTPUT_DIR"
cp "../buildroot/output/images/boot.bin" "$OUTPUT_DIR"
cp "../buildroot/output/images/u-boot.img" "$OUTPUT_DIR"
cp "../buildroot/output/images/piksiv3.dtb" "$OUTPUT_DIR"
cp "../buildroot/output/images/zImage" "$OUTPUT_DIR"

# TODO: Download and copy FPGA and firmware binaries
