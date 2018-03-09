#!/bin/ash

skylark_enabled() {
  if [ x`cat /var/run/skylark_enabled` != "x0" ]; then
    return 0
  else
    return 1
  fi
}

ntrip_enabled() {
  if [ x`cat /var/run/ntrip_enabled` != "x0" ]; then
    return 0
  else
    return 1
  fi
}

should_warn_on_no_inet() {
  if skylark_enabled || ntrip_enabled; then
    return 0
  else
    return 1
  fi
}

while true; do
  if ! should_warn_on_no_inet; then
    sleep 1
    continue
  else
    sleep 10
  fi
  if [ x`cat /var/run/network_available` != "x1" ]; then
    echo "No route to Internet" | sbp_log --warn
  fi
done &

child_pid=$!
trap 'kill $child_pid; exit' EXIT STOP TERM

while true; do
  if ping -w 5 -c 1 8.8.8.8 >/dev/null 2>&1 || \
     ping -w 5 -c 1 114.114.114.114 > /dev/null 2>&1; then
    echo 1 > /var/run/network_available
    sleep 10
  else
    echo 0 > /var/run/network_available
    sleep 1
  fi
done
