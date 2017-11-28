#!/bin/bash

set -euo pipefail
IFS=$'\n\t'

D=$( (cd "$(dirname "$0")" >/dev/null; pwd -P) )

VERSION_TAG=$(cat $D/version_tag)
DOCKER_REPO_NAME=swiftnav/buildroot-base
DOCKER_USER=swiftnav

build_dir=$(mktemp -d)
trap 'rm -rfv $build_dir' EXIT

cp -v "$D/Dockerfile.base" "${build_dir}"
cd "${build_dir}"

docker build \
  --force-rm \
  --no-cache \
  -f Dockerfile.base \
  -t $DOCKER_REPO_NAME:$VERSION_TAG \
  .

docker login --username="${DOCKER_USER:-swiftnav}" --password="$DOCKER_PASS"
docker push "$DOCKER_REPO_NAME:$VERSION_TAG"
