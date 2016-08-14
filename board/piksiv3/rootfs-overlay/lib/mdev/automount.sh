#!/bin/sh

destdir=/media

do_umount()
{
  if grep -qs "^/dev/$1 " /proc/mounts ; then
    umount "${destdir}/$1";
  fi

  [ -d "${destdir}/$1" ] && rmdir "${destdir}/$1"
}

do_mount()
{
  mkdir -p "${destdir}/$1" || exit 1

  if ! mount -t auto -o sync "/dev/$1" "${destdir}/$1"; then
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

