#!/bin/sh

otaserver="192.168.0.100:8000"
uuid=`cat /factory/uuid`
check_interval=30

while (true)
do
  current_version=`cat /img_tbl/boot/name`
  want_version=`curl -s $otaserver/ota/$uuid/version`
  
  echo "Current firmware version:" [$current_version]
  echo "Want version:" [$want_version]

  if [ "$current_version" != "$want_version" ]; then
    echo "Downloading firmware" $want_version
    echo "Downloading firmware" $want_version | sbp_log --info
    curl -s -o /tmp/update $otaserver/ota/$uuid/update
    upgrade_tool --debug /tmp/update | sbp_log --info
    reboot -f
  fi 

  sleep $check_interval

done

