#!/bin/sh

DURO_EEPROM_PATH="/sys/devices/soc0/amba/e0005000.i2c/i2c-1/1-0050/eeprom"
DURO_EEPROM_TMP_PATH="/cfg/duro_eeprom.tmp"
DURO_EEPROM_CFG_PATH="/cfg/duro_eeprom"
EEPROM_RETRY_DELAY=0.15 # seconds
DEVICE_DURO_ID_STRING="DUROV"

log_tag=copy_duro_eeprom

source /etc/init.d/common.sh
source /etc/init.d/logging.sh

setup_loggers

try_eeprom_copy()
{
  dd if=$DURO_EEPROM_PATH bs=1 count=6 of=$DURO_EEPROM_TMP_PATH &>/dev/null
}

copy_duro_eeprom()
{
  local retries=5
  if [ -f "$DURO_EEPROM_PATH" ]; then
    while [ $retries -ge 0 ]; do
      if try_eeprom_copy; then
        logi "'Duro' EEPROM read to $DURO_EEPROM_TMP_PATH."
        break;
      fi
      loge "Failed to copy Duro EEPROM, ${retries} retries left..."
      retries=$(($retries-1))
      sleep $EEPROM_RETRY_DELAY
    done
  else
    logw "No 'Duro' EEPROM path present on this device."
  fi

  # create and setup permission for file regardless
  # File existing is an indication that this script is done reading EEPROM

  touch "$DURO_EEPROM_TMP_PATH"
  chmod 0644 "$DURO_EEPROM_TMP_PATH"
  logi "Moving $DURO_EEPROM_TMP_PATH to $DURO_EEPROM_CFG_PATH."
  mv $DURO_EEPROM_TMP_PATH $DURO_EEPROM_CFG_PATH

  # Parse eeprom and set is_duro flag if string "duro" appears in eeprom.
  # Downstream systems can either read EEPROM contents or check is_duro flag.
  # pfwp uses is_duro flag as an IPC method to enable can termination setting

  if grep -q "^${DEVICE_DURO_ID_STRING}" $DURO_EEPROM_CFG_PATH; then
    echo "1" > /etc/flags/is_duro
  else
    echo "0" > /etc/flags/is_duro
  fi
}

copy_duro_eeprom
