#!/bin/ash

while true; do
  sleep 10
  if [ x`cat /var/run/network_available` != "x1" ]; then
    echo "No route to Internet" | sbp_log --warn
  fi
done &

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
