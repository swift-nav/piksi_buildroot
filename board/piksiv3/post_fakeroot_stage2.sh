#!/bin/bash

set -e
set -x

if [ -z "$HW_CONFIG" ]; then
    echo "ERROR: HW_CONFIG is not set"
    exit 1
fi

if [ -n "$DEBUG" ]; then
  LOG_DEBUG=/dev/stdout
else
  LOG_DEBUG=/dev/null
fi

ROOTFS_DIR=$1
rm -rf $ROOTFS_DIR/dev
