#!/bin/bash

# Copyright (C) 2016-2018 Swift Navigation Inc.
# Contact: Swift Navigation <dev@swiftnav.com>
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
[[ -z "$DEBUG" ]] || set -x

if [ "$TRAVIS_OS_NAME" != "linux" ]; then
    exit
fi

REPO="${PWD##*/}"
BUCKET="${BUCKET:-swiftnav-artifacts}"
PRS_BUCKET="${PRS_BUCKET:-swiftnav-artifacts-pull-requests}"

BUILD_VERSION=${BUILD_VERSION:-$(git describe --tags --dirty --always)}
BUILD_PATH="$REPO/$BUILD_VERSION"

BUILD_SOURCE="pull-request"

SCENARIO="live-roof-650-townsend-post"
STATUS_HITL_CONTEXT="hitl/pass-fail"

if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
    HITL_BUILD_SOURCE="master"
else
    HITL_BUILD_SOURCE="pr"
fi

hitl_viewer_link() {
    local PAGE=$1
    local ARGS=$2
    echo "https://gnss-analysis.swiftnav.com$PAGE/$ARGS"
}

hitl_metrics_link() {
    local PRESET=$1
    echo $(hitl_viewer_link "" "metrics_preset=$PRESET&scenario=$SCENARIO&build_type=$HITL_BUILD_SOURCE&firmware_versions=$BUILD_VERSION")
}

hitl_pass_fail_link () {
    echo $(hitl_metrics_link "pass_fail")
}

hitl_artifacts_link () {
    echo $(hitl_viewer_link "/artifacts" "build_type=$HITL_BUILD_SOURCE&name_filter=$BUILD_VERSION")
}

echo "Comment PULL_REQUEST ($TRAVIS_PULL_REQUEST)"
echo "Comment BRANCH ($TRAVIS_BRANCH)"
echo "Comment TAG ($TRAVIS_TAG)"

if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
    if [[ "$TRAVIS_BRANCH" == master || "$TRAVIS_TAG" == v* || "$TRAVIS_BRANCH" == v*-release ]]; then
        COMMENT="$BUILD_PATH
https://console.aws.amazon.com/s3/home?region=us-west-2&bucket=swiftnav-artifacts&prefix=$BUILD_PATH/"
        URL="https://slack.com/api/chat.postMessage?token=$SLACK_TOKEN&channel=$SLACK_CHANNEL"
        DATA="text=$COMMENT"
        curl --data-urlencode "$DATA" "$URL"
    fi
elif [ ! -z "$GITHUB_TOKEN" ]; then
    COMMENT=\
"## $BUILD_VERSION\n"\
"+ [Artifacts (HITL Dashboard Page)]($(hitl_artifacts_link))\n"\
"+ [Artifacts (S3 / AWS Console)](https://console.aws.amazon.com/s3/home?region=us-west-2&bucket=swiftnav-artifacts-pull-requests&prefix=$BUILD_PATH/)\n"\
"+ s3://$PRS_BUCKET/$BUILD_PATH"
    URL="https://api.github.com/repos/swift-nav/$REPO/issues/$TRAVIS_PULL_REQUEST/comments"
    curl -u "$GITHUB_TOKEN:" -X POST "$URL" -d "{\"body\":\"$COMMENT\"}"

    STATUS_URL="https://api.github.com/repos/swift-nav/$REPO/statuses/$TRAVIS_PULL_REQUEST_SHA"
    STATUS_DESCRIPTION="Waiting for HITL tests to be run and complete"
    STATUS_TARGET_URL=$(hitl_pass_fail_link)
    STATUS_STATE="pending"

    curl -i -X POST -u "$GITHUB_TOKEN:" $STATUS_URL -d "{\"state\": \"$STATUS_STATE\",\"target_url\": \"$STATUS_TARGET_URL\", \"description\": \"$STATUS_DESCRIPTION\", \"context\": \"$STATUS_HITL_CONTEXT\"}"
fi

