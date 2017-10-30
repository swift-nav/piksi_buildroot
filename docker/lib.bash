#!/bin/bash

set -euo pipefail
IFS=$'\n\t'

sha1sum() {
  python -c '\
    h = __import__("hashlib").new("sha1"); \
    h.update(__import__("sys").stdin.read()); \
    print(h.hexdigest())' < <(cat $*)
}

do_config_hash() {

  HASHED_CONFIGS=(board/piksiv3/linux.config board/piksiv3/busybox.config 
    configs/piksiv3_defconfig)

  BR_DOCKER_VERSION=$(sha1sum ${HASHED_CONFIGS[*]})
  BR_DOCKER_NAMETAG="swiftnav/piksi_buildroot:${BR_DOCKER_VERSION}"
}
