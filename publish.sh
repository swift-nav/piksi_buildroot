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

BUILD_VERSION="$(git describe --tags --dirty --always)"
BUILD_PATH="$REPO/$BUILD_VERSION"
if [[ ! -z "$PRODUCT_VERSION" ]]; then
    BUILD_PATH="$BUILD_PATH/$PRODUCT_VERSION"
fi

echo "Uploading $@ to $BUILD_PATH"

for file in "$@"
do
    KEY="$BUILD_PATH/$(basename $file)"
    if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
        if [ "$TRAVIS_BRANCH" == "master" ]; then
            OBJECT="s3://$BUCKET/$KEY"
            aws s3 cp "$file" "$OBJECT"
        fi
    else
        aws s3api put-object --no-sign-request --bucket "$PRS_BUCKET" --key "$KEY" --body "$file" --acl public-read
    fi
done
