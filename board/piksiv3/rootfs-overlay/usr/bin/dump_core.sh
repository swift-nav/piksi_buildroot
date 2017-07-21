#!/bin/sh
/usr/bin/logger "CORE DUMP!! executable=$1 pid=$2 time=$3"
echo "CORE DUMP!! executable=$1 pid=$2 time=$3" | /usr/bin/sbp_log --error

corefile="/tmp/cores/core-$1-$2-$3"
exec cat >> $corefile

