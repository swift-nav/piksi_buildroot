#!/bin/ash

## Resources and configuration:

default_check_period=10
default_retry_period=1
default_ping_addresses=8.8.8.8

recheck_enabled_period=1

skylark_enabled_file=/var/run/skylark/enabled
ntrip_enabled_file=/var/run/ntrip/enabled

# Files managed by piksi_system_daemon:
piksi_sys_dir=/var/run/piksi_sys
polling_period_file=$piksi_sys_dir/network_polling_period
polling_retry_file=$piksi_sys_dir/network_polling_retry_period
polling_addresses_file=$piksi_sys_dir/network_polling_addresses
network_available_file=$piksi_sys_dir/network_available
ping_log_enabled_file=$piksi_sys_dir/enable_ping_logging

ping_log_file=/var/log/ping.log

## Functions:

skylark_enabled()
{
  if ! [[ -f $skylark_enabled_file ]]; then
    return 0
  fi

  if [ x`cat $skylark_enabled_file` != "x0" ]; then
    return 0
  else
    return 1
  fi
}

ntrip_enabled()
{
  if ! [[ -f $ntrip_enabled_file ]]; then
    return 0
  fi

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

warn_check_disabled_logged=

reset_warn_check_disabled()
{
  warn_check_disabled_logged=
}

warn_check_disabled()
{
  if [[ -n "$warn_check_disabled_logged" ]]; then
    return 0
  fi

  echo "Network status LED disabled (network check frequency set to zero)." | \
    sbp_log --warn

  warn_check_disabled_logged=y
}

check_enabled()
{
  if [[ -z "$(cat "$polling_period_file")" ]]; then
    return 1
  else
    return 0
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

ping_addesses()
{
  local addresses
  addresses="$(cat "$polling_addresses_file")"

  if [[ -n "$addresses" ]]; then
    echo "$default_ping_addresses" | tr ',' '\n'
  fi

  echo "$addresses" | tr ',' '\n' 
}

run_ping()
{
  local IFS=$'\n'

  for address in $(ping_addesses); do
    if ping -w 5 -c 1 "$address" >>"$(ping_log)" 2>&1; then
      return 0
    fi
  done

  return 1
}

## Main script:

while true; do
  if ! should_warn_on_no_inet; then
    sleep 1
    continue
  else
    sleep 10
  fi
  if ! check_enabled; then
    warn_check_disabled
    continue
  fi
  reset_warn_check_disabled
  if [ x`cat $network_available_file` != "x1" ]; then
    echo "No route to Internet" | sbp_log --warn
  fi
done &

child_pid=$!
trap 'kill $child_pid; exit' EXIT STOP TERM HUP

while true; do
  if ! check_enabled; then
    echo 0 >$network_available_file
    sleep $recheck_enabled_period
    continue
  fi
  log_start
  if run_ping; then
    log_stop
    echo 1 >$network_available_file
    sleep_connectivity_check
  else
    log_failed_stop
    echo 0 >$network_available_file
    sleep_connectivity_retry
  fi
done
