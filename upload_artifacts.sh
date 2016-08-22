#!/bin/bash

# This script is used in .travis.yml

set -e

BUCKET=piksi-buildroot-images
PIKSI_VERSION=v3
BUILD_VERSION=$(git describe --tags --dirty --always)

FOLDER=$TRAVIS_BRANCH
if [ "$TRAVIS_PULL_REQUEST" == "true" ]; then
  $FOLDER="pull_requests/$FOLDER"
fi

BUILD_DIR="$(date -u +%Y-%m-%dT%H:%M:%SZ)_${TRAVIS_BUILD_NUMBER}_${BUILD_VERSION}"

UPLOAD_DIR="s3://$BUCKET/$PIKSI_VERSION/$FOLDER/"

FILES="boot.bin u-boot.img piksiv3.dtb zImage"

echo "Uploading images to $UPLOAD_DIR"
for file in $FILES
do
  aws s3 cp --no-sign-request "./buildroot/output/images/$file" "$UPLOAD_DIR"
done
