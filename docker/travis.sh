#!/bin/bash

D=$( (cd "$(dirname "$0")" >/dev/null; pwd -P) )

if [ "$TRAVIS_PULL_REQUEST" != "false" ]; then
  file_names=`curl "https://api.github.com/repos/$TRAVIS_REPO_SLUG/pulls/$TRAVIS_PULL_REQUEST/files" | jq '.[] | .filename' | tr '\n' ' ' | tr '"' ' '`
else
  file_names=`(git diff --name-only $TRAVIS_COMMIT_RANGE || echo "") | tr '\n' ' '`
fi

if echo $file_names | grep -q "Dockerfile.base"; then
	$D/base.bash
fi
