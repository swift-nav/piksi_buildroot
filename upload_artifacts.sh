#!/bin/bash

# This script is used in .travis.yml

set -e

BUCKET=piksi-buildroot-images
PIKSI_VERSION=v3
BUILD_VERSION=$(git describe --tags --dirty --always)

if [ "$TRAVIS_PULL_REQUEST" == "true" ]; then
  FOLDER=pull_requests
else
  if [ "$TRAVIS_BRANCH" == "master" ]; then
    FOLDER=master
  else
    FOLDER=misc
  fi
fi

BUILD_DIR="UTC-$(date -u +%Y-%m-%dT%H:%M:%SZ)_$(echo $TRAVIS_BUILD_NUMBER)_$(echo $BUILD_VERSION)"

mkdir -p uploads/$BUILD_DIR

UPLOAD_DIR="s3://$BUCKET/$PIKSI_VERSION/$FOLDER/"

cp "./buildroot/output/images/boot.bin" "./uploads/$BUILD_DIR"
cp "./buildroot/output/images/u-boot.img" "./uploads/$BUILD_DIR"
cp "./buildroot/output/images/piksiv3.dtb" "./uploads/$BUILD_DIR"
cp "./buildroot/output/images/zImage" "./uploads/$BUILD_DIR"

echo "Uploading images to $UPLOAD_DIR"
aws s3 cp --recursive --no-sign-request ./uploads/ $UPLOAD_DIR
