#!/bin/ash

name="migrate_sdcard"
log_tag=$name

source /etc/init.d/sdcard.sh
source /etc/init.d/logging.sh

setup_loggers

partition_disk()
{
  local dev="$1"; shift

  sed -e 's/\s*\([\+0-9a-zA-Z]*\).*/\1/' <<EOF | fdisk $dev
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

format_with_f2fs()
{
  local dev="/dev/$1"; shift
  local fstype=$(lsblk -o FSTYPE $dev | tail -n -1)

  logi "Existing fs type: ${fstype}"

  local pkname=$(lsblk -o PKNAME $dev | tail -n -1)

  logi "Creating partition table on ${pkname}..."
  partition_disk /dev/$pkname

  # Force the kernel to re-read the partition table
  blockdev --rereadpt /dev/$pkname

  logi "Formatting partition with F2FS..."
  mkfs -t f2fs $dev || logw "Formatting failed"
}

list_partitions()
{
  lsblk -o NAME,TYPE | grep "$MOUNTNAME" | grep ' part$' | awk '{print $1}' | tail -n -1
}

wait_for_sdcard_mount

for dev in $(list_partitions); do
  devname=${dev:2}
  if needs_migration $devname; then

    # Stop services that use the sdcard
    /etc/init.d/S83standalone_file_logger stop
    /etc/init.d/S98copy_sys_logs stop

    # Disable automount...
    echo >/var/run/automount_disabled

    mountpoint=$(lsblk -o MOUNTPOINT /dev/$devname | tail -n -1)
    [[ -z "$mountpoint" ]] || umount $mountpoint

    logw "Migrating '${devname}' to F2FS..."
    format_with_f2fs $devname

    reboot -f
  fi
done
