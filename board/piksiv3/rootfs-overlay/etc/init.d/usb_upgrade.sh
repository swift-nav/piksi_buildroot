#!/bin/ash

export FIRMWARE="/media/sda*/PiksiMulti-*.bin"
export LOGLEVEL="--warn"

_dir_wait () {
    [[ $# -lt 3 ]] && {
        echo "Usage: ${FUNCNAME} <num_timeout> <len_timeout_ms> <path>"; return 1
    }
    RET=1
    REPEATS="${1}"
    LEN_TIMEOUT_MS="${2}"
    MNT_PATH="${3}"
    until [ -x $MNT_PATH ] || [ $REPEATS -lt 1 ]; do
       usleep $(( $LEN_TIMEOUT_MS * 1000 ))
       let REPEATS=$REPEATS-1
    done
}

# try and mount drive for up to 5 seconds or until success 
_dir_wait 20 250 $FIRMWARE

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
# Killing monit and USB logger
monit stop standalone_file_logger
monit stop zmq_adapter_rpmsg_piksi100
upgrade_tool $FIRMWARE | sbp_log $LOGLEVEL
RETVAL=$?
umount /media/sda1
sync
if [ $RETVAL -eq 0 ]; then
  while [ 1 ]; do
    echo "Upgrade completed successfully. Please remove upgrade media and reboot."  | sbp_log $LOGLEVEL
    echo "Upgrade completed successfully. Please remove upgrade media and reboot."
    sleep 1
  done
fi
if [ $RETVAL -ne 0 ]; then
  while [ 1 ]; do
    echo "ERROR: Upgrade was unsuccessful. Please verify the image and try again."  | sbp_log $LOGLEVEL
    echo "ERROR: Upgrade was unsuccessful. Please verify the image and try again."
    sleep 1
  done
fi
