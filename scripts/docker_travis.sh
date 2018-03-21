#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'

D=$( (cd "$(dirname "$0")" || exit 1 >/dev/null; pwd -P) )

if [[ "$TRAVIS_PULL_REQUEST" != "false" ]]; then

  files_url="https://api.github.com/repos/$TRAVIS_REPO_SLUG/pulls/$TRAVIS_PULL_REQUEST/files"
  file_names=$( curl -sSL "$files_url" | jq '.[] | .filename' | tr '"' ' ' )

else
  file_names=$( git diff --name-only "$TRAVIS_COMMIT_RANGE" || echo "" )
fi

echo ">>> Files associated with current build request:"
echo "$file_names"

echo "..."

if echo "$file_names" | grep -q -E "/Dockerfile[.]base"; then
	"$D/docker_base.bash"
else
  echo "No modifications to base Docker image found..."
fi
