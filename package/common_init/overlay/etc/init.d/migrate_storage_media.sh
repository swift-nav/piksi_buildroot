#!/bin/ash

# shellcheck disable=SC2039,SC2169,SC1091

[[ -z "${DEBUG:-}" ]] || set -x
#export DEBUG_SDCARD=y

export name="migrate_sdcard"
export log_tag=$name

source /etc/init.d/storage_media.sh
source /etc/init.d/logging.sh

setup_loggers

partition_disk()
{
  local dev="$1"; shift

  logd "partition_disk: dev=$dev"

  sed -e 's/\s*\([\+0-9a-zA-Z]*\).*/\1/' <<EOF | fdisk "$dev"
      o # clear the in memory partition table
      n # new partition
      p # primary partition
      1 # partition number 1
        # default - start at beginning of disk
        # default - use whole disk
      w # write the partition table
      q # quit
EOF
}

format_with_fs_type()
{
  local dev="/dev/$1"; shift
  local new_fs_type=$1; shift

  logd "format_with_fs_type: dev=$dev, new_fs_type=$new_fs_type"

  local fstype
  fstype=$(lsblk -o FSTYPE "$dev" | tail -n -1)

  logi "Existing fs type: ${fstype}"

  local pkname
  pkname=$(lsblk -o PKNAME "$dev" | tail -n -1)

  logi "Creating partition table on ${pkname}..."
  partition_disk "/dev/$pkname"

  # Force the kernel to re-read the partition table
  blockdev --rereadpt "/dev/$pkname"

  logi "Formatting partition with ${new_fs_type}..."
  if [[ "$new_fs_type" == "$STORAGE_TYPE_F2FS" ]]; then
    mkfs.f2fs "$dev" || logw "Formatting F2FS failed"
  elif [[ "$new_fs_type" == "$STORAGE_TYPE_NTFS" ]]; then
    mkfs.ntfs --fast "$dev" || logw "Formatting NTFS failed"
  else
    loge "Unknown filesystem type: ${new_fs_type}"
    return 1
  fi

  return 0
}

list_partitions()
{
  local mountname=$1; shift
  lsblk -o NAME,TYPE | grep "$mountname" | grep ' part$' | awk '{print $1}' | tail -n -1
}

set_reboot_after_migrate()
{
  echo >/var/run/reboot_after_migrate
}

reboot_after_migrate()
{
  [[ -f /var/run/reboot_after_migrate ]]
}

migrate_storage()
{
  local mountname=$1
  logd "migrate_storage: $mountname"

  wait_for_mount "$mountname"

  for dev in $(list_partitions "$mountname"); do
    devname="${dev:2}"
    if needs_migration "$devname"; then

      # Stop services that use the sdcard
      /etc/init.d/S83standalone_file_logger stop
      /etc/init.d/S98copy_sys_logs stop

      # Disable automount...
      echo >/var/run/automount_disabled

      mountpoint=$(lsblk -o MOUNTPOINT "/dev/$devname" | tail -n -1)

      [[ -z "$mountpoint" ]] || umount "$mountpoint" || {
        logw --sbp "Failed to migrate '${devname}' to ${new_fs_type}..."
        return 1; 
      }

      new_fs_type=$(fetch_new_fs_type "$devname")
      logw --sbp "Migrating '$devname' to $new_fs_type..."

      format_with_fs_type "$devname" "$new_fs_type" || return 1

      logi  "Done migrating '${devname}' to ${new_fs_type}..."

      set_reboot_after_migrate
      set_stop_wait_for_mount
    fi
  done
}

migrate_sdcard()
{
  logd "migrate_sdcard"
  migrate_storage "$SDCARD_MOUNTNAME"
}

migrate_usb_drive()
{
  logd "migrate_usb_drive"
  migrate_storage "$USB_DRIVE_MOUNTNAME"
}

migrate_sdcard &
migrate_sdcard_pid=$!

migrate_usb_drive &
migrate_usb_drive_pid=$!

wait $migrate_sdcard_pid $migrate_usb_drive_pid

if reboot_after_migrate; then

  logw --sbp "Done migrating storage media, rebooting..."
  sleep 0.1

  reboot -f

else
  logi "Done migrating storage media..."
fi
