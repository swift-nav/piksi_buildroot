#!/bin/ash

should_check_inet() {
  [ x`cat /var/run/skylark_enabled` != "x0" ] || \
    [ x`cat /var/run/ntrip_enabled` != "x0" ]
}

while true; do
  if ! should_check_inet; then
    sleep 1
    continue
  else
    sleep 10
  fi
  if [ x`cat /var/run/network_available` != "x1" ]; then
    echo "No route to Internet" | sbp_log --warn
  fi
done &

while true; do
  if ! should_check_inet; then
    sleep 1
    continue
  fi
  if ping -w 5 -c 1 8.8.8.8 >/dev/null 2>&1 || \
     ping -w 5 -c 1 114.114.114.114 > /dev/null 2>&1; then
    echo 1 > /var/run/network_available
    sleep 10
  else
    echo 0 > /var/run/network_available
    sleep 1
  fi
done
