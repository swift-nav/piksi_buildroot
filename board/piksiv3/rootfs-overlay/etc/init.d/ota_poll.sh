#!/bin/ash

## Resources and configuration:

check_period=1

polling_period_file=/dev/null
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
    echo 'OTA poll'
    date
  else
  fi
  sleep $check_period
done
