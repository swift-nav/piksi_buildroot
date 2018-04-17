#!/bin/ash

source /etc/init.d/sdcard.sh
log_dir="$MOUNTPOINT/logs"

log_tag=copy_sys_logs
source /etc/init.d/logging.sh

should_run() {
  if needs_migration $MOUNTNAME; then
    logd "Exiting because the sdcard needs to be migrated..."
    return 0
  fi
  if [[ -n "${COPY_SYS_LOGS}" ]]; then
    return 1
  fi
  if [[ grep -q "copy_system_logs=True" /persistent/config.ini ]]; then
    return 1
  fi
  return 0
}

wait_for_sdcard_mount

if ! should_run; then
  exit 0
fi

mkdir -p "$log_dir"

N=0
while [[ -d "$log_dir/$N" ]] ; do
  N=$(($N+1))
done

cleanup_rsync() {
  [[ -z "$rsync_pid" ]] || kill "$rsync_pid"
}

trap 'cleanup_loggers; cleanup_rsync; exit 0' EXIT TERM INT

mkdir "$log_dir/$N"

while true; do
  rsync -r /var/log/ "$log_dir/$N/" &
  rsync_pid=$!
  wait "$rsync_pid"
  sleep 1
done
