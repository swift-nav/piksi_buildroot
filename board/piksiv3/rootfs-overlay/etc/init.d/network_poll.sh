#!/bin/ash

## Resources and configuration:

default_check_period=10
default_retry_period=1

skylark_enabled_file=/var/run/skylark/enabled
ntrip_enabled_file=/var/run/ntrip/enabled

# Files managed by piksi_system_daemon:
piksi_sys_dir=/var/run/piksi_sys
polling_period_file=$piksi_sys_dir/network_polling_period
polling_retry_file=$piksi_sys_dir/network_polling_retry_period
network_available_file=$piksi_sys_dir/network_available
ping_log_enabled_file=$piksi_sys_dir/enable_ping_logging

ping_log_file=/var/log/ping.log

## Functions:

skylark_enabled()
{
  if [ x`cat $skylark_enabled_file` != "x0" ]; then
    return 0
  else
    return 1
  fi
}

ntrip_enabled()
{
  if [ x`cat $ntrip_enabled_file` != "x0" ]; then
    return 0
  else
    return 1
  fi
}

ping_logging_enabled()
{
  if ! [[ -f $ping_log_enabled_file ]]; then
    return 1
  fi

  if [[ -z "$(cat $ping_log_enabled_file)" ]]; then
    return 1;
  fi

  return 0
}

ping_log()
{
  if ping_logging_enabled; then
    echo $ping_log_file
  else
    echo /dev/null
  fi
}

should_warn_on_no_inet()
{
  if skylark_enabled || ntrip_enabled; then
    return 0
  else
    return 1
  fi
}

sleep_connectivity_check()
{
  period=$(cat $polling_period_file)
  [[ -n "$period" ]] || period=$default_check_period

  sleep $period
}

sleep_connectivity_retry()
{
  period=$(cat $polling_retry_file)
  [[ -n "$period" ]] || period=$default_retry_period

  sleep $period
}

log_start()
{
  echo "--- PING START: $(date -Is)" >>$(ping_log)
}

log_failed_stop()
{
    echo "--- PING STOP (failed): $(date -Is)" >>$(ping_log)
}

log_stop()
{
  echo "--- PING STOP: $(date -Is)" >>$(ping_log)
}

## Main script:

while true; do
  if ! should_warn_on_no_inet; then
    sleep 1
    continue
  else
    sleep 10
  fi
  if [ x`cat $network_available_file` != "x1" ]; then
    echo "No route to Internet" | sbp_log --warn
  fi
done &

child_pid=$!
trap 'kill $child_pid; exit' EXIT STOP TERM HUP

while true; do
  log_start
  if ping -w 5 -c 1 8.8.8.8 >>$(ping_log) 2>&1 || \
     ping -w 5 -c 1 114.114.114.114 >>$(ping_log) 2>&1; then
    log_stop
    echo 1 >$network_available_file
    sleep_connectivity_check
  else
    log_failed_stop
    echo 0 >$network_available_file
    sleep_connectivity_retry
  fi
done
