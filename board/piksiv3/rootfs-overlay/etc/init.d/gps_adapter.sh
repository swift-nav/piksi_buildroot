#!/bin/ash

fifo=/var/run/gps_ntp/adapter

rm -f $fifo
mkfifo $fifo

cleanup()
{
  rm $fifo
  kill $zmq_adapter_pid $socat_pid

  exit 0
}

trap 'cleanup' HUP TERM

zmq_adapter --stdio -s '>tcp://127.0.0.1:44030' >$fifo &
zmq_adapter_pid=$!

socat pty,raw,link=/tmp/tmp.gps0 fd:0 <$fifo &
socat_pid=$!

wait $zmq_adapter_pid $socat_pid
