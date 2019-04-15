# shellcheck disable=SC2039

_mount_base="/media"

if grep -q "root=/dev/mmcblk0" /proc/device-tree/chosen/bootargs; then
  SDCARD_MOUNTNAME="nonexistant"
else
	SDCARD_MOUNTNAME="mmcblk0p1"
fi

export SDCARD_MOUNTNAME
export USB_DRIVE_MOUNTNAME="sda1"

export SDCARD_MOUNTPOINT="$_mount_base/$SDCARD_MOUNTNAME"
export USB_DRIVE_MOUNTPOINT="$_mount_base/$USB_DRIVE_MOUNTNAME"

export STORAGE_TYPE_F2FS="F2FS"
export STORAGE_TYPE_NTFS="NTFS"

# Wait 30 seconds for the sdcard to be mounted
_wait_mount_limit=30

_sdcard_debug_log()
{
  [[ -z "${DEBUG_SDCARD:-}" ]] || logd "$*"
}

_is_mounted()
{
  local mountname=$1; shift
  local mountpoint=$_mount_base/$mountname

  _sdcard_debug_log "_is_mounted: $mountpoint"

  grep -q "$mountpoint" /proc/mounts
}

_inotify_wait_mountpoint()
{
  local mountname=$1; shift
  local mountpoint=$_mount_base/$mountname

  _sdcard_debug_log "_inotify_wait_mountpoint: $mountpoint"

  [[ -e "$mountpoint" ]] || inotifywait -e create "$_mount_base" -q -t 1 \
    | grep -q "CREATE $mountname"
}

_seconds_since_start() {

  local the_time
  the_time=$(cut -f1 -d' ' </proc/uptime)

  _sdcard_debug_log "_seconds_since_start"

  echo "${the_time/.*}"
}

set_stop_wait_for_mount()
{
  echo >/var/run/stop_wait_for_mount
}

stop_wait_for_mount()
{
  [[ -f /var/run/stop_wait_for_mount ]]
}

wait_for_mount()
{
  local mountname=$1; shift
  local mountpoint=$_mount_base/$mountname

  _sdcard_debug_log "wait_for_mount: $mountpoint"

  local start_time=
  local current_time=

  start_time=$(_seconds_since_start)

  while ! _inotify_wait_mountpoint "$mountname"; do

    current_time=$(_seconds_since_start)

    if [[ $(( current_time - start_time )) -gt $_wait_mount_limit ]]; then
      logi "Ending wait for $mountname (inotify)..."
      return 0
    fi

    if stop_wait_for_mount; then
      logi "Wait for $mountname stopped (inotify)..."
      return 0
    fi

    continue;
  done

  while ! _is_mounted "$mountname"; do

    current_time=$(_seconds_since_start)

    if [[ $(( current_time - start_time )) -gt $_wait_mount_limit ]]; then
      logi "Ending wait for $mountname (_is_mounted)..."
      return 0
    fi

    if stop_wait_for_mount; then
      logi "Wait for $mountname stopped (_is_mounted)..."
      return 0
    fi

    sleep 0.5;
    continue;
  done

  if ! [[ -d "$mountpoint" ]]; then
    loge "Mountpoint exists, but is not a directory"
  fi
}

wait_for_sdcard_mount()
{
  _sdcard_debug_log "wait_for_sdcard_mount"
  wait_for_mount "$SDCARD_MOUNTNAME"
}

wait_for_usb_drive_mount()
{
  _sdcard_debug_log "wait_for_usb_drive_mount"
  wait_for_mount "$USB_DRIVE_MOUNTNAME"
}

_is_f2fs_enabled()
{
  _sdcard_debug_log "_is_f2fs_enabled"
  [[ "$(query_config --section standalone_logging --key logging_file_system)" == "F2FS" ]]
}

_is_ntfs_enabled()
{
  _sdcard_debug_log "_is_ntfs_enabled"
  [[ "$(query_config --section standalone_logging --key logging_file_system)" == "NTFS" ]]
}

fetch_new_fs_type()
{
  local devname=$1; shift
  _sdcard_debug_log "fetch_new_fs_type: $devname"

  if _is_f2fs_enabled; then
    if detect_f2fs "$devname"; then
      echo ""
    else
      echo "$STORAGE_TYPE_F2FS"
    fi
  elif _is_ntfs_enabled; then
    if detect_ntfs "$devname"; then
      echo ""
    else
      echo "$STORAGE_TYPE_NTFS"
    fi
  else
      echo ""
  fi
}

needs_migration()
{
  local devname=$1; shift
  _sdcard_debug_log "needs_migration: $devname"

  local dev="/dev/$mountname"

  if ! [[ -e $dev ]]; then
    _sdcard_debug_log "needs_migration: device not present: $dev"
    return 1 # no
  fi

  if ! _is_f2fs_enabled && ! _is_ntfs_enabled; then
    _sdcard_debug_log "needs_migration: did not detect F2FS or NTFS in config"
    return 1 # no
  fi

  if _is_f2fs_enabled; then
    if detect_f2fs "$devname"; then
      _sdcard_debug_log "needs_migration: already F2FS: $dev"
      return 1 # no
    fi
  fi

  if _is_ntfs_enabled; then
    if detect_ntfs "$devname"; then
      _sdcard_debug_log "needs_migration: already NTFS: $dev"
      return 1 # no
    fi
  fi

  _sdcard_debug_log "needs_migration: need migration: $dev"
  return 0 # yes
}

_detect_fs_type()
{
  local devname=$1; shift
  local desired_fs_type=$1; shift

  _sdcard_debug_log "_detect_fs_type: devname=$devname, desired_fs_type=$desired_fs_type"

  local dev="/dev/$devname"
  local fstype

  fstype=$(lsblk "$dev" -o FSTYPE | tail -n -1)

  _sdcard_debug_log "_detect_fs_type: file-system type: '$fstype'"

  if [[ "$fstype" == "$desired_fs_type" ]]; then
    return 0
  fi

  return 1
}

detect_f2fs()
{
  local devname=$1; shift
  _sdcard_debug_log "detect_f2fs: $devname"

  local ls_blk_str=f2fs
  _detect_fs_type "$devname" "$ls_blk_str"
}

detect_ntfs()
{
  local devname=$1; shift
  _sdcard_debug_log "detect_ntfs: $devname"

  local ls_blk_str=ntfs
  _detect_fs_type "$devname" "$ls_blk_str"
}
