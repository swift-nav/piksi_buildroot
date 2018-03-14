#!/bin/sh

#cp -al / /stage1
#
#rm -rf /stage1/etc/passwd
#rm -rf /stage1/lib/modules
#rm -rf /stage1/dev
#
#rm -rf /stage1/stage1
#rm -rf /stage1/stage2
#rm -rf /stage1/merged

mkdir /overlay

mount -t tmpfs -o mode=0755 tmpfs /overlay

mkdir -p /overlay/upperdir/dev
mkdir -p /overlay/upperdir/dev/pts
mkdir -p /overlay/upperdir/dev/shm
mkdir -p /overlay/upperdir/proc
mkdir -p /overlay/upperdir/sys
mkdir -p /overlay/upperdir/sys/kernel/config
mkdir -p /overlay/upperdir/tmp
mkdir -p /overlay/upperdir/run

mkdir /overlay/upperdir
mkdir /overlay/workdir
