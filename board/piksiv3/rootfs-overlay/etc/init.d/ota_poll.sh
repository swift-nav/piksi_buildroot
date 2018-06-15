#!/bin/ash

log_tag=ota
source /etc/init.d/logging.sh
setup_loggers

## Resources and configuration:

check_period=1800

ota_enabled_file=/var/run/ota_enabled

## Functions:

ota_enabled()
{
  if ! [[ -f $ota_enabled_file ]]; then
    return 1
  fi

  if [[ -z "$(cat $ota_enabled_file)" ]]; then
    return 1;
  fi

  return 0
}

## Main script:

child_pid=$!
trap 'kill $child_pid; exit' EXIT STOP TERM

while true; do
  if ota_enabled; then
    ota_tool `cat /cfg/device_uuid` \
             `cat /img_tbl/boot/name`
    sleep $check_period
  else
    sleep 10
  fi
done
