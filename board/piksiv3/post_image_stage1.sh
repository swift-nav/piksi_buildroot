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

IMAGES_DIR=$1
ROOTFS=$IMAGES_DIR/../target

rm -rf $ROOTFS/*

BACKUP_DIR=/tmp/$(echo $PWD | sed 's@/@_@g')

mkdir -p $ROOTFS
rsync -a --delete $BACKUP_DIR/ $ROOTFS/
