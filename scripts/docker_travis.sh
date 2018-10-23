#!/usr/bin/env bash

[[ -z "$DEBUG" ]] || set -x

set -euo pipefail
IFS=$'\n\t'

D=$( (cd "$(dirname "$0")" || exit 1 >/dev/null; pwd -P) )

pr_files_json=$(mktemp)
pr_files=$(mktemp)

trap 'rm -f $pr_files_json $pr_files' EXIT

fetch_pr_files() {

  local retry_count=5
  local sleep_seconds=1
  local got_files=

  local files_url="https://api.github.com/repos/$TRAVIS_REPO_SLUG/pulls/$TRAVIS_PULL_REQUEST/files"

  echo ">>> Will use the following URL to check the files in the PR:"
  echo $'\t'"$files_url"

  for _ in $(seq $retry_count); do
    if ! curl -sSL "$files_url" --output "$pr_files_json"; then
      echo "WARNING: cURL failed, sleeping..." >&2
      sleep $(( sleep_seconds + ($((RANDOM)) % 5) ))
      continue
    fi
    if ! jq '.[] | .filename' "$pr_files_json" | tr '"' ' ' >"$pr_files"; then
      echo "WARNING: Got unexpected JSON blob, sleeping..." >&2
      sleep $(( sleep_seconds + ($((RANDOM)) % 5) ))
    fi
    got_files=y
    break
  done

  if [[ -z "$got_files" ]]; then
    echo "ERROR: failed to get JSON blob for PR..." >&2
    exit 1
  fi
}

if [[ "$TRAVIS_PULL_REQUEST" != "false" ]]; then
  fetch_pr_files
else
  ( git diff --name-only "$TRAVIS_COMMIT_RANGE" || echo "" ) >"$pr_files"
fi

echo ">>> Detecting files associated with current build request"
cat "$pr_files"

if grep -q -E "/Dockerfile[.]base" "$pr_files"; then
	"$D/docker_base.bash"
else
  echo ">>> No modifications to base Docker image found"
fi
