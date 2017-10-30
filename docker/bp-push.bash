#!/bin/bash

D=$( (cd "$(dirname "$0")" || exit 1 >/dev/null; pwd -P) )

# shellcheck source=lib.bash
source "$D/lib.bash"

DOCKER_USER=swiftnav
do_config_hash

set -euo pipefail
IFS=$'\n\t'

if [[ -z "${HW_CONFIG:-}" ]]; then
  HW_CONFIG=prod
fi

docker login --username="$DOCKER_USER" --password="$DOCKER_PASS"
docker push "$BR_DOCKER_NAMETAG"
