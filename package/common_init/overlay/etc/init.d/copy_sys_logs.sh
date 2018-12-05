#!/bin/ash

log_tag=copy_sys_logs

source /etc/init.d/sdcard.sh
source /etc/init.d/logging.sh

setup_loggers

should_run()
{
  if needs_migration $MOUNTNAME; then
    logw --sbp "Exiting: the SD card needs to be migrated..."
    return 1
  fi

  if [[ -n "${COPY_SYS_LOGS}" ]]; then
    # If COPY_SYS_LOGS is not empty, we were run dynamically as
    #   the result of a settings value change...
    return 0
  fi

  if ! grep -q "copy_system_logs=True" /persistent/config.ini; then
    logd --sbp "Exiting: logging is not enabled..."
    return 1
  fi

  # Setting is on and persisted, we should start
  return 0
}

wait_for_sdcard_mount

if ! should_run; then
  exit 0
fi

log_dir="$MOUNTPOINT/logs"
mkdir -p "$log_dir"

N=1
while true; do
  log_dir_n="$log_dir/$(printf '%04d' $N)"
  if ! [[ -d "$log_dir_n" ]]; then
    break;
  fi
  N=$(($N+1))
done

cleanup_rsync()
{
  [[ -z "$rsync_pid" ]] || kill "$rsync_pid"
}

trap 'cleanup_loggers; cleanup_rsync; exit 0' EXIT TERM STOP HUP INT

mkdir "$log_dir_n"

while true; do
  rsync --exclude=tmp.* -r /var/log/ "$log_dir_n" &
  rsync_pid=$!
  wait "$rsync_pid"
  sync "$log_dir_n"
  sleep 1
done
