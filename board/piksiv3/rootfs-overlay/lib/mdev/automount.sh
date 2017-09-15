#!/bin/sh

destdir=/media

do_umount()
{
  if grep -qs "^/dev/$1 " /proc/mounts ; then
    umount -f "${destdir}/$1";
  fi

  [ -d "${destdir}/$1" ] && rmdir "${destdir}/$1"
}

detect_fat()
{
  # Detect a FAT filesystem, the `file' utility will spit out output like
  #   this for device files:
  #
  # /dev/mmcblk0p1: DOS/MBR boot sector, code offset 0x58+2, OEM-ID "BSD  4.4",
  #    sectors/cluster 32, Media descriptor 0xf8, sectors/track 32, heads 255,
  #    hidden sectors 8192, sectors 62513152 (volumes > 32 MB) , FAT (32 bit),
  #    sectors/FAT 15255, reserved 0x1, serial number 0xc60f130c,
  #    label: "S90        "

  dev="/dev/$1"

  if file -s $dev | grep -q 'FAT .*, sectors/FAT.*'; then
    return 0
  fi

  return 1
}

do_mount()
{
  mkdir -p "${destdir}/$1" || exit 1

  if detect_fat $1; then
    fsck.vfat -fy "/dev/$1" || echo "automount.sh: fsck.vfat failed"
  fi

  if ! mount -t auto "/dev/$1" "${destdir}/$1"; then
    # failed to mount, clean up mountpoint
    rmdir "${destdir}/$1"
    exit 1
  fi
}

case "${ACTION}" in
add|"")
  do_umount ${MDEV}
  do_mount ${MDEV}
  ;;
remove)
  do_umount ${MDEV}
  ;;
esac

