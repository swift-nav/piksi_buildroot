#!/bin/bash

D=$( (cd "`dirname $0`" >/dev/null; pwd -P) )
source "$D/lib.bash"

do_config_hash

ROOT=/piksi_buildroot

if [[ -z "${HW_CONFIG:-}" ]]; then
  HW_CONFIG=prod
fi

docker run \
  -i -t --rm \
  -e HW_CONFIG=$HW_CONFIG \
  -e BR2_EXTERNAL=/piksi_buildroot \
  -e BR2_DL_DIR=/piksi_buildroot/output/dl \
  --name "piksi_br_prebuild" \
  -v $PWD:$ROOT \
  -v piksi_bp-output:$ROOT/output \
  -v $PWD/output/target/images:$ROOT/output/target/images \
  $BR_DOCKER_NAMETAG \
  $*
