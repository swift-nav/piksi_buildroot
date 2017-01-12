#!/bin/sh

name="zmq_adapter_uart_$ttydev"
cmd="zmq_adapter --file /dev/$ttydev -p >tcp://127.0.0.1:43031 -s >tcp://127.0.0.1:43030 -f sbp $filter_options"
dir="/"
user=""

#source /etc/init.d/template_process.inc.sh

