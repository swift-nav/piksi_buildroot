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

function usage () {
    echo "Usage: $0 <VERSION> [FILES]"
    exit -1
}

REPO="${PWD##*/}"
BUCKET="${BUCKET:-swiftnav-artifacts}"

PRODUCT_VERSION="$1"
if [[ -z "$PRODUCT_VERSION" ]]; then
    usage
fi

BUILD_VERSION="$(git describe --tags --dirty --always)"
BUILD_PATH="$REPO/$BUILD_VERSION/$PRODUCT_VERSION"

FILES=${@:2}

echo "Uploading $FILES to $BUILD_PATH"

for file in $FILES
do
    object="s3://$BUCKET/$BUILD_PATH/$(basename $file)"
    if [[ -z "$ANONYMOUS" ]]; then
        aws s3 cp "$file" "$object"
    else
        aws s3 cp --no-sign-request "$file" "$object"
    fi
done
