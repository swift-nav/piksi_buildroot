#!/bin/bash

# Used in .travis.yml

BUCKET=piksi-buildroot-images
PIKSI_VERSION=v3

set -e

# Install the AWS command line interface
curl "https://s3.amazonaws.com/aws-cli/awscli-bundle.zip" -o "awscli-bundle.zip"
unzip awscli-bundle.zip
sudo ./awscli-bundle/install -i /usr/local/aws -b /usr/local/bin/aws

BUILD_VERSION=$(git rev-parse --short HEAD)
BUILD_DIR=commit_$BUILD_VERSION
mkdir -p $BUILD_DIR

# "folders" on S3 are prefix keys
aws s3api put-object --bucket $BUCKET --key $PIKSI_VERSION/$BUILD_DIR/
UPLOAD_DIR="$BUCKET/$PIKSI_VERSION/$BUILD_DIR"

cp "./output/images/boot.bin" "$BUILD_DIR"
cp "./output/images/u-boot.img" "$BUILD_DIR"
cp "./output/images/piksiv3.dtb" "$BUILD_DIR"
cp "./output/images/zImage" "$BUILD_DIR"

echo "Uploading images to $UPLOAD_DIR"
aws s3 cp $BUILD_DIR s3://$UPLOAD_DIR --include '*'
