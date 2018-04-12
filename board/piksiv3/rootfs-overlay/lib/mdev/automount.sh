#!/bin/sh

destdir=/media

needs_migration()
{
  if ! grep -q "logging_file_system=F2FS" /persistent/config.ini; then
    return 1
  fi

  if detect_f2fs $1; then
    return 1
  fi

  return 0
}

detect_f2fs()
{
  local dev="/dev/$1"
  local f2fs_ident='F2FS.*'

  if file -s $dev | grep -q "$f2fs_ident"; then
    return 0
  fi

  return 1
}

do_umount()
{
  if grep -qs "^/dev/$1 " /proc/mounts ; then
    umount -f "${destdir}/$1";
  fi

  [ -d "${destdir}/$1" ] && rmdir "${destdir}/$1"
}

do_mount()
{
  mkdir -p "${destdir}/$1" || exit 1

  if needs_migration $1; then
      migrate_to_f2fs $1
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

