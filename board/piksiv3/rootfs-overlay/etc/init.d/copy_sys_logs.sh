#!/bin/ash

export LOGDIR="/media/mmcblk0p1/logs"
export LOGLEVEL="--info"
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

# try and mount drive for up to 10 seconds or until success 
_dir_wait 20 500 $LOGDIR

if [ ! -x $LOGDIR ]; then
  echo "No log directory found on SD Card"
  sleep 1
  exit
fi

echo "Log Directory Found on SD Card; logs will be periodically copied." | sbp_log $LOGLEVEL
echo "Log Directory Found on SD Card; logs will be periodically copied."

N=0
while [[ -d $LOGDIR/$N ]] ; do
    N=$(($N+1))
done

mkdir $LOGDIR/$N

while true; do
          #cp -R /var/log/* $LOGDIR/$N
          rsync -r /var/log/ $LOGDIR/$N
          sleep 1
done
