#!/bin/bash

set -e
set -x

exit 0

#if [ -z "$HW_CONFIG" ]; then
#    echo "ERROR: HW_CONFIG is not set"
#    exit 1
#fi
#
#if [ -n "$DEBUG" ]; then
#  LOG_DEBUG=/dev/stdout
#else
#  LOG_DEBUG=/dev/null
#fi
#
#IMAGES_DIR=$1
#FIRMWARE_DIR=$BASE_DIR/../../firmware
#
#if [[ ! -f $FIRMWARE_DIR/stage2.squashfs ]]; then
#    cp $IMAGES_DIR/rootfs.squashfs $FIRMWARE_DIR/stage2.squashfs
#    exit 0
#fi
#
#exit 0
