#!/bin/ash

fifo=/var/run/gps_ntp/adapter

rm -f $fifo
mkfifo $fifo

cleanup()
{
  rm $fifo
  kill "$endpoint_adapter_pid" "$socat_pid"
}

trap 'cleanup; exit 0' EXIT TERM HUP

endpoint_adapter --retry --name gps --stdio -s 'ipc:///var/run/sockets/nmea_external.pub' >$fifo &
endpoint_adapter_pid=$!

socat pty,raw,link=/tmp/tmp.gps0 fd:0 <$fifo &
socat_pid=$!

wait $endpoint_adapter_pid $socat_pid
