#!/bin/bash

# Copyright (C) 2016 Swift Navigation Inc.
# Contact: Mark Fine <mark@swiftnav.com>
#
# This source is subject to the license found in the file 'LICENSE' which must
# be be distributed together with this source. All other rights reserved.
#
# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
# EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
#
# Script for publishing built binaries to S3.

set -e

if [ "$TRAVIS_OS_NAME" != "linux" ]; then
    exit
fi

REPO="${PWD##*/}"
BUCKET="${BUCKET:-swiftnav-artifacts}"
PRS_BUCKET="${PRS_BUCKET:-swiftnav-artifacts-pull-requests}"

BUILD_VERSION="${BUILD_VERSION:-$(git describe --tags --dirty --always)}"
BUILD_PATH="$REPO/$BUILD_VERSION"
if [[ ! -z "$PRODUCT_VERSION" ]]; then
    BUILD_PATH="$BUILD_PATH/$PRODUCT_VERSION"
fi
if [[ ! -z "$PRODUCT_REV" ]]; then
    BUILD_PATH="$BUILD_PATH/$PRODUCT_REV"
fi
if [[ ! -z "$PRODUCT_TYPE" ]]; then
    BUILD_PATH="$BUILD_PATH/$PRODUCT_TYPE"
fi

echo "Uploading $@ to $BUILD_PATH"
echo "Publish PULL_REQUEST ($TRAVIS_PULL_REQUEST)"
echo "Publish BRANCH ($TRAVIS_BRANCH)"
echo "Publish TAG ($TRAVIS_TAG)"

for file in "$@"; do
    KEY="$BUILD_PATH/$(basename $file)"
    if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
        if [[ "$TRAVIS_BRANCH" == master || "$TRAVIS_TAG" == v* || "$TRAVIS_BRANCH" == v*-release ]]; then
            OBJECT="s3://$BUCKET/$KEY"
            echo "Pushing to $OBJECT"
            aws s3 cp "$file" "$OBJECT"
        fi
    else
        OBJECT="s3://$PRS_BUCKET/$KEY"
        echo "Pushing to $OBJECT"
        aws s3 cp "$file" "$OBJECT"
    fi
done
