#!/usr/bin/env bash

set -e
set -x

if [ -z "$HW_CONFIG" ]; then
  echo "ERROR: HW_CONFIG is not set"
  exit 1
fi

ROOTFS=$1

FIRMWARE_DIR_ROOTFS=$ROOTFS/lib/firmware
FIRMWARE_DIR=$BASE_DIR/../../firmware

BACKUP_DIR=/tmp/$(echo $PWD | sed 's@/@_@g')
rm -rf $BACKUP_DIR

mkdir -p $BACKUP_DIR
rsync -a $ROOTFS/ $BACKUP_DIR/

rm -rf $FIRMWARE_DIR/modules
rsync -a $ROOTFS/lib/modules $FIRMWARE_DIR/.

rm -rf $ROOTFS/lib/modules/*
rm -rf $ROOTFS/lib/debug/*

#rm -rf $ROOTFS/*
#
#mkdir -p $ROOTFS/work
#mkdir -p $ROOTFS/fs
#
#rsync -a $BACKUP_DIR/ $ROOTFS/fs/
#
#pushd $ROOTFS
#
#mkdir sbin
#ln -s fs/sbin/init sbin/init
#
#mkdir etc
#ln -s fs/etc/inittab etc/inittab
#
#popd

# Stage the stage2 squashfs if it exists
if [[ -f $FIRMWARE_DIR/stage2.squashfs ]]; then
    cp $FIRMWARE_DIR/stage2.squashfs $ROOTFS/.
fi
