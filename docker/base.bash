#!/bin/bash

set -euo pipefail
IFS=$'\n\t'

VERSION_TAG=2017.11.13
DOCKER_REPO_NAME=swiftnav/buildroot-base
DOCKER_USER=swiftnav

D=$( (cd "$(dirname "$0")" >/dev/null; pwd -P) )

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
