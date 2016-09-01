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

if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
    COMMENT="$BUILD_VERSION
https://console.aws.amazon.com/s3/home?region=us-west-2&bucket=swiftnav-artifacts&prefix=$BUILD_PATH/"
    URL="https://slack.com/api/chat.postMessage?token=$SLACK_TOKEN&channel=$SLACK_CHANNEL"
    DATA="text=$COMMENT"
    curl --data-urlencode "$DATA" "$URL"
elif [ ! -z "$GITHUB_TOKEN" ]; then
    COMMENT="## $BUILD_VERSION\n+ [s3://$PRS_BUCKET/$BUILD_PATH](https://console.aws.amazon.com/s3/home?region=us-west-2&bucket=swiftnav-artifacts-pull-requests&prefix=$BUILD_PATH/)"
    URL="https://api.github.com/repos/swift-nav/$REPO/issues/$TRAVIS_PULL_REQUEST/comments"
    curl -u "$GITHUB_TOKEN:" -X POST "$URL" -d "{\"body\":\"$COMMENT\"}"
fi

