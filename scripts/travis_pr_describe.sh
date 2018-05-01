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

D=$( (cd "$(dirname "$0")" || exit 1 >/dev/null; pwd -P) )
DESCRIPTION_FILE="$D/../pr_description.yaml"

# TRAVIS_BRANCH:
#   for push builds, or builds not triggered by a pull request, this is the name of the branch.
#   for builds triggered by a pull request this is the name of the branch targeted by the pull request.
#   Note that for tags, git does not store the branch from which a commit was tagged.
# TRAVIS_COMMIT: The commit that the current build is testing.
# TRAVIS_COMMIT_MESSAGE: The commit subject and body, unwrapped.
# TRAVIS_COMMIT_RANGE: The range of commits that were included in the push or pull request. (Note that this is empty for builds triggered by the initial commit of a new branch.)
# TRAVIS_PULL_REQUEST: The pull request number if the current job is a pull request, “false” if it’s not a pull request.
# TRAVIS_PULL_REQUEST_BRANCH:
#   if the current job is a pull request, the name of the branch from which the PR originated.
#   if the current job is a push build, this variable is empty ("").
# TRAVIS_PULL_REQUEST_SHA:
#   if the current job is a pull request, the commit SHA of the HEAD commit of the PR.
#   if the current job is a push build, this variable is empty ("").
# TRAVIS_PULL_REQUEST_SLUG:
#   if the current job is a pull request, the slug (in the form owner_name/repo_name) of the repository from which the PR originated.
#   if the current job is a push build, this variable is empty ("").
# TRAVIS_REPO_SLUG: The slug (in form: owner_name/repo_name) of the repository currently being built.
# TRAVIS_TEST_RESULT: is set to 0 if the build is successful and 1 if the build is broken.
# TRAVIS_TAG: If the current build is for a git tag, this variable is set to the tag’s name.

echo "---
commit:
  sha: $TRAVIS_COMMIT
  message: $TRAVIS_COMMIT_MESSAGE
  range: $TRAVIS_COMMIT_RANGE
pr:
  number: $TRAVIS_PULL_REQUEST
  source_branch: $TRAVIS_PULL_REQUEST_BRANCH
  sha: $TRAVIS_PULL_REQUEST_SHA
  source_slug: $TRAVIS_PULL_REQUEST_SLUG
target:
  branch: $TRAVIS_BRANCH
  slug: $TRAVIS_REPO_SLUG
test_result: $TRAVIS_TEST_RESULT
tag: $TRAVIS_TAG
" > $DESCRIPTION_FILE
$D/../publish.sh $DESCRIPTION_FILE
