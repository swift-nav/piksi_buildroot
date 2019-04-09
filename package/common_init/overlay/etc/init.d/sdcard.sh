MOUNT_BASE="/media"
if grep -q "root=/dev/mmcblk0" /proc/device-tree/chosen/bootargs; then
	MOUNTNAME="nonexistant"
else
	MOUNTNAME="mmcblk0p1"
fi
MOUNTPOINT="$MOUNT_BASE/$MOUNTNAME"

# Wait 30 seconds for the sdcard to be mounted
wait_sdcard_limit=30

is_mounted()
{
  grep -q $MOUNTPOINT /proc/mounts
}

inotify_wait_mountpoint()
{
  [[ -e "$MOUNTPOINT" ]] || inotifywait -e create "$MOUNT_BASE" -q -t 1 | grep -q "CREATE $MOUNTNAME"
}

seconds_since_start() {
  local the_time=$(cut -f1 -d' ' </proc/uptime)
  echo ${the_time/.*}
}

wait_for_sdcard_mount()
{
  local start_time=$(seconds_since_start)
  local current_time=

  while ! inotify_wait_mountpoint; do

    current_time=$(seconds_since_start)
    if [[ $(( current_time - start_time )) -gt $wait_sdcard_limit ]]; then
      logi "Ending wait for sdcard mount..."
      exit 0
    fi

    continue;
  done

  while ! is_mounted; do

    current_time=$(seconds_since_start)
    if [[ $(( current_time - start_time )) -gt $wait_sdcard_limit ]]; then
      logi "Ending wait for sdcard mount..."
      exit 0
    fi

    sleep 0.5;
    continue;
  done

  if ! [[ -d "$MOUNTPOINT" ]]; then
    logw "Mountpoint exists, but is not a directory"
  fi
}

is_f2fs_enabled()
{
  [[ "$(query_config --section standalone_logging --key logging_file_system)" == "F2FS" ]]
}

is_ntfs_enabled()
{
  [[ "$(query_config --section standalone_logging --key logging_file_system)" == "NTFS" ]]
}

fetch_new_fs_type()
{
  local devname=$1; shift

  if is_f2fs_enabled; then
    if detect_f2fs "$devname"; then
      echo ""
    else
      echo "f2fs"
    fi
  elif is_ntfs_enabled; then
    if detect_ntfs "$devname"; then
      echo ""
    else
      echo "ntfs"
    fi
  else
      echo ""
  fi
}

needs_migration()
{
  local devname=$1; shift

  if ! is_f2fs_enabled && ! is_ntfs_enabled; then
    logd "Did not detect F2FS or NTFS in config"
    return 1 # no
  fi

  if is_f2fs_enabled; then
    if detect_f2fs $devname; then
      return 1 # no
    fi
  fi

  if is_ntfs_enabled; then
    if detect_ntfs $devname; then
      return 1 # no
    fi
  fi

  return 0 # yes
}

detect_fs_type()
{
  local devname=$1; shift
  local desired_fs_type=$1; shift

  local dev="/dev/$devname"
  local fstype

  fstype=$(lsblk "$dev" -o FSTYPE | tail -n -1)

  logd "detect_fs_type: file-system type: '$fstype'"

  if [[ "$fstype" == "$desired_fs_type" ]]; then
    return 0
  fi

  return 1
}

detect_f2fs()
{
  local devname=$1; shift
  detect_fs_type "$devname" "f2fs"
}

detect_ntfs()
{
  local devname=$1; shift
  detect_fs_type "$devname" "ntfs"
}
