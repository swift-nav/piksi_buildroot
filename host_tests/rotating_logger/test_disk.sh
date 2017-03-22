#!/bin/bash

IMG_FILE=$1
MNT_DIR=$2

if [ -e "$1" ]
then
  sudo umount "$MNT_DIR"
  rm "$IMG_FILE"
else
  fallocate -l 8M "$IMG_FILE"
  mkfs.ext4 "$IMG_FILE"
  mkdir -p "$MNT_DIR"
  sudo mount -t auto -o loop "$IMG_FILE" "$MNT_DIR"
  sudo chmod -R 777 "$MNT_DIR"
fi

