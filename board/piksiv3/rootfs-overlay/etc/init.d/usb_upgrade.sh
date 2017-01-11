#!/bin/ash

export FIRMWARE="/media/sda1/PiksiMulti-*.bin"
export LOGLEVEL="--warn"

_dir_wait () {
    [[ $# -lt 3 ]] && { 
        echo "Usage: ${FUNCNAME} <num_timeout> <len_timeout_ms> <path>"; return 1
    }
    RET=1
    REPEATS="${1}"
    LEN_TIMEOUT_MS="${2}"
    MNT_PATH="${4}"
    until [ -x $FIRMWARE ] || [ $REPEATS -lt 1 ]; do
       usleep $(( $LEN_TIMEOUT_MS * 1000 ))
       let REPEATS=$REPEATS-1
    done
}

# try and mount drive for up to 5 seconds or until success 
_dir_wait 20 250 /media/sda1

if [ `echo $FIRMWARE | wc -w` != '1' ]; then
  echo 'Unable to perform upgrade: exactly one firmware image is required' | sbp_log $LOGLEVEL
  echo 'Unable to perform upgrade: exactly one firmware image is required'
  exit
fi

if [ ! -x $FIRMWARE ]; then
  echo "No fw found"
  sleep 1
  exit
fi

echo "New firmware image set detected: `ls $FIRMWARE`" | sbp_log $LOGLEVEL
echo "New firmware image set detected: `ls $FIRMWARE`"
echo "Performing upgrade..." |  sbp_log $LOGLEVEL
echo "Performing upgrade..."
# Killing any processes that could hurt user experience
/etc/init.d/S90monit stop
/etc/init.d/S83zmq_adapter_rpmsg_piksi100 stop
upgrade_tool --debug $FIRMWARE | sbp_log $LOGLEVEL
umount /media/sda1
sync
while [ 1 ]; do 
  echo "Please remove upgrade media and reboot."  | sbp_log $LOGLEVEL
  echo "Please remove upgrade media and reboot." 
  sleep 1
done
