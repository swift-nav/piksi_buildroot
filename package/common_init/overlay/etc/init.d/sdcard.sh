MOUNT_BASE="/media"
MOUNTNAME="mmcblk0p1"
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

needs_migration()
{
  local devname=$1; shift

  if ! is_f2fs_enabled; then
    logd "Did not detect F2FS in config"
    return 1 # no
  fi

  if detect_f2fs $devname; then
    return 1 # no
  fi

  return 0 # yes
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
