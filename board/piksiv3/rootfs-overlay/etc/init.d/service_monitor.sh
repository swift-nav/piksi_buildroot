#!/bin/ash

log_tag=svcmon

source /etc/init.d/common.sh
source /etc/init.d/logging.sh

check_period_sec=3

check_service()
{
  local service_name=$1; shift
  local pid_file=$1; shift
  local service_control=$1; shift

  if ! is_running $pid_file; then
    logw --sbp "Service '$service_name' is not running, restarting (PID file was '$pid_file')..."
    sudo $service_control start
  fi
}

monitor_services()
{
  [ -f /etc/release_lockdown ] || check_service sshd /var/run/dropbear.pid /etc/init.d/S50dropbear
  check_service syslogd /var/run/syslogd.pid /etc/init.d/S01logging
  check_service klogd /var/run/klogd.pid /etc/init.d/S01logging
}

while true; do
  monitor_services
  sleep $check_period_sec
done
