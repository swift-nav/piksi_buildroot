#!/usr/bin/env bash

[[ -z "$DEBUG" ]] || set -x

set -euo pipefail
IFS=$'\n\t'

D=$( (cd "$(dirname "$0")" || exit 1 >/dev/null; pwd -P) )

pr_files_json=$(mktemp)
#trap 'rm -f $pr_files_json' EXIT

fetch_pr_files_json() {

  local retry_count=5
  local sleep_seconds=1
  local got_json=

  local files_url="https://api.github.com/repos/$TRAVIS_REPO_SLUG/pulls/$TRAVIS_PULL_REQUEST/files"

  echo ">>> Will use the following URL to check the files in the PR:"
  echo $'\t'"$files_url"

  for i in $(seq $retry_count); do
    curl -sSL "$files_url" --output "$pr_files_json"
    if [[ $? -ne 0 ]]; then
      echo "WARNING: cURL failed, sleeping..." >&2
      sleep $(( $sleep_seconds + ($((RANDOM)) % 5) ))
      continue
    fi
    got_json=y
    break
  done

  if [[ -z "$got_json" ]]; then
    echo "ERROR: failed to get JSON blob for PR..." >&2
    exit 1
  fi
}

if [[ "$TRAVIS_PULL_REQUEST" != "false" ]]; then
  fetch_pr_files_json
  file_names=$(jq '.[] | .filename' $pr_files_json | tr '"' ' ')
else
  file_names=$( git diff --name-only "$TRAVIS_COMMIT_RANGE" || echo "" )
fi

echo ">>> Detecting files associated with current build request"
echo "$file_names"

if echo "$file_names" | grep -q -E "/Dockerfile[.]base"; then
	"$D/docker_base.bash"
else
  echo ">>> No modifications to base Docker image found"
fi
