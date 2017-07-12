#!/bin/ash

export RESETFILE="/media/sda*/reset2defaults.txt"
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
_dir_wait 20 250 $RESETFILE

if [ ! -x $RESETFILE ]; then
  echo "No reset to defaults file found"
  sleep 1
  exit
fi

echo "reset2defaults.txt file found on flashdrive.  Resetting Piksi to default settings" | sbp_log $LOGLEVEL
echo "reset2defaults.txt file found on flashdrive.  Resetting Piksi to default settings" 
rm -rf /persistent/*
rm $RESETFILE
echo "Rebooting Piksi." | sbp_log $LOGLEVEL
echo "Rebooting Piksi." 
reboot
