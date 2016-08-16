#!/bin/bash

# Used in .travis.yml

BUCKET=piksi-buildroot-images
PIKSI_VERSION=v3

set -e

BUILD_VERSION=$(git rev-parse --short HEAD)
BUILD_DIR=commit_$BUILD_VERSION
# "folders" on S3 are prefix keys
# http://stackoverflow.com/questions/33113354/how-to-create-a-folder-like-i-e-prefix-object-on-s3-using-the-aws-cli
aws s3api put-object --bucket $BUCKET --key $PIKSI_VERSION/$BUILD_DIR/
UPLOAD_DIR="s3://$BUCKET/$PIKSI_VERSION/$BUILD_DIR"

echo "Uploading images to $UPLOAD_DIR"
aws s3 cp "./output/images/boot.bin" "$UPLOAD_DIR"
aws s3 cp "./output/images/u-boot.img" "$UPLOAD_DIR"
aws s3 cp "./output/images/piksiv3.dtb" "$UPLOAD_DIR"
aws s3 cp "./output/images/zImage" "$UPLOAD_DIR"
