#!/bin/ash

export FIRMWARE="/media/sda*/PiksiMulti-*.bin"
export LOGLEVEL="--warn"

UT_RET=UPGRADE_ERROR_UNKNOWN,UPGRADE_ERROR_OPTIONS
UT_RET=${UT_RET},UPGRADE_ERROR_IN_PROGRESS,UPGRADE_ERROR_PART_INFO_POP
UT_RET=${UT_RET},UPGRADE_ERROR_PART_INFO_VERIFY,UPGRADE_ERROR_TARGET_PARAMS_GET
UT_RET=${UT_RET},UPGRADE_ERROR_DATA_LOAD,UPGRADE_ERROR_DATA_VERIFY,UPGRADE_ERROR_FACTORY_DATA
UT_RET=${UT_RET},UPGRADE_ERROR_INVALID_HARDWARE,UPGRADE_ERROR_IMAGE_TABLE
UT_RET=${UT_RET},UPGRADE_ERROR_IMAGE_POPULATE,UPGRADE_ERROR_UPGRADE_INSTALL

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
# Killing realtime fw messages and USB logger
/etc/init.d/S83endpoint_adapter_rpmsg_piksi100 stop
/etc/init.d/S83standalone_file_logger stop
upgrade_tool_output=$(upgrade_tool $FIRMWARE) 
RETVAL=$?
echo $upgrade_tool_output | sbp_log $LOGLEVEL
echo $upgrade_tool_output
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
    echo "$(echo ${UT_RET} | cut -d',' -f$RETVAL): Upgrade was unsuccessful. Please verify the image and reboot."  | sbp_log $LOGLEVEL
    echo "$(echo ${UT_RET} | cut -d',' -f$RETVAL): Upgrade was unsuccessful. Please verify the image and reboot."
    sleep 1
  done
fi
