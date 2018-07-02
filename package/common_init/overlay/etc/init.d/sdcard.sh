MOUNT_BASE="/media"
MOUNTNAME="mmcblk0p1"
MOUNTPOINT="$MOUNT_BASE/$MOUNTNAME"

is_mounted()
{
  grep -q $MOUNTPOINT /proc/mounts
}

inotify_wait_mountpoint()
{
  [[ -e "$MOUNTPOINT" ]] || inotifywait -e create "$MOUNT_BASE" -q -t 1 | grep -q "CREATE $MOUNTNAME"
}

wait_for_sdcard_mount()
{
  while ! inotify_wait_mountpoint; do
    continue;
  done
  while ! is_mounted; do
    sleep 0.5; continue;
  done
  if ! [[ -d "$MOUNTPOINT" ]]; then
    logw "Mountpoint exists, but is not a directory"
  fi
}

needs_migration()
{
  local devname=$1; shift

  if ! grep -q "logging_file_system=F2FS" /persistent/config.ini; then
    logd "Did not detect F2FS in config"
    return 1
  fi

  if detect_f2fs $devname; then
    return 1
  fi

  return 0
}

detect_f2fs()
{
  local devname=$1; shift

  local dev="/dev/$devname"
  local fstype=$(lsblk $dev -o FSTYPE | tail -n -1)

  logd "detect_f2fs: file-system type: '$fstype'"

  if [[ "$fstype" == "f2fs" ]]; then
    return 0
  fi

  return 1
}
