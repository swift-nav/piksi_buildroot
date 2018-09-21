#!/bin/ash

log_tag=ifplugd_sh
source /etc/init.d/logging.sh

setup_loggers

interface=eth0
poll_delay=1
up_delay=1
down_delay=10
restart_delay=1

cleanup()
{
  logi "stopping..."

  [[ -z "$pid" ]] || \
    kill -TERM $pid

  sudo ifplugd -k
}

trap 'cleanup_loggers; cleanup; exit 0' HUP TERM STOP EXIT

run_ifplugd()
{
  local opts=

  opts="$opts -I "             # Don't exit on nonzero exit code from script
  opts="$opts -q "             # Don't run "down" script on exit
  opts="$opts -p "             # Don't run "up" script on start-up
  opts="$opts -n "             # Don't daemonize
  opts="$opts -i $interface "  # The interface to monitor
  opts="$opts -t $poll_delay " # How often to poll the device
  opts="$opts -u $up_delay "   # How long to wait before running "up" script
  opts="$opts -d $down_delay " # How long to wait before running "down" script

  sudo ifplugd $opts
}

(
  logi "starting..."

  while true; do

    run_ifplugd; status=$?

    if [[ $status -ne 0 ]]; then
      loge "ifplugd exited unexpectedly"
    else
      logw "ifplugd reported a link detection error: $?"
    fi

    sleep $restart_delay
    logi "restarting..."

  done
)&

pid=$!
wait $pid
