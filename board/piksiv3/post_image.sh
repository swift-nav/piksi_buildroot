#!/usr/bin/env bash

[[ -z "${DEBUG:-}" ]] || set -x

set -euo pipefail
IFS=$'\n\t'

if [[ -z "${HW_CONFIG:-}" ]]; then
  echo "ERROR: HW_CONFIG is not set"
  exit 1
fi

if [[ -n "${DEBUG:-}" ]]; then
  LOG_DEBUG=/dev/stdout
else
  LOG_DEBUG=/dev/null
fi

if [[ -z "${BR2_EXTERNAL_piksi_buildroot_PATH:-}" ]]; then
  echo "ERROR: the variable 'BR2_EXTERNAL_piksi_buildroot_PATH' cannot be empty"
fi

bold=$(tput rev 2>/dev/null || :)
normal=$(tput sgr0 2>/dev/null || :)

CFG=piksiv3_$HW_CONFIG
get_git_string_script="$(dirname "$0")"/get_git_string.sh
GIT_STRING=$($get_git_string_script)
# remove the githash from the filename for useability
FILE_GIT_STRING=${GIT_STRING%%-g*}

OUTPUT_DIR=$BINARIES_DIR/${CFG}
FIRMWARE_DIR=$TARGET_DIR/lib/firmware

UBOOT_MK_PATH="$BR2_EXTERNAL_piksi_buildroot_PATH/package/uboot_custom/uboot_custom.mk"

UBOOT_VERSION_REGEX='UBOOT_CUSTOM_VERSION *= *'
UBOOT_VERSION=$(grep "$UBOOT_VERSION_REGEX" $UBOOT_MK_PATH | sed "s/${UBOOT_VERSION_REGEX}\\(.*\\)/\\1/")

UBOOT_BASE_DIR=$(find $BUILD_DIR -maxdepth 1 -type d -name "uboot_custom-${UBOOT_VERSION}")

DEV_BIN_PATH=$OUTPUT_DIR/PiksiMulti-DEV-$FILE_GIT_STRING.bin
FAILSAFE_BIN_PATH=$OUTPUT_DIR/PiksiMulti-FAILSAFE-$FILE_GIT_STRING.bin
INTERNAL_BIN_PATH=$OUTPUT_DIR/PiksiMulti-INTERNAL-$FILE_GIT_STRING.bin
REL_OPEN_BIN_PATH=$OUTPUT_DIR/PiksiMulti-UNPROTECTED-$FILE_GIT_STRING.bin
REL_PROT_BIN_PATH=$OUTPUT_DIR/PiksiMulti-$FILE_GIT_STRING.bin

if [[ -z "${UBOOT_BASE_DIR:-}" ]]; then
  echo "ERROR: Could not find uboot directory" >&2
  exit 1
fi

UBOOT_PROD_DIR=$UBOOT_BASE_DIR/build/${CFG}_prod
UBOOT_FAILSAFE_DIR=$UBOOT_BASE_DIR/build/${CFG}_failsafe
UBOOT_DEV_DIR=$UBOOT_BASE_DIR/build/${CFG}_dev

LOG_FILE=$(mktemp)
trap 'rm -rf $LOG_FILE' EXIT

report_failsafe_loc()
{
  echo -e "${bold}>>> FAILSAFE firmware image located at:${normal}\n\t$FAILSAFE_BIN_PATH"
}

generate_dev()
{
  echo -n ">>> Generating DEV firmware image... "

  # Ensure log is empty
  true >$LOG_FILE

  $UBOOT_DEV_DIR/tools/mkimage                                                \
  -A arm -T firmware -C none -O u-boot -n "FPGA bitstream"                    \
  -d $FIRMWARE_DIR/piksi_fpga.bit                                             \
  $OUTPUT_DIR/piksi_fpga.img                                                  \
    | tee -a $LOG_FILE &>$LOG_DEBUG

  if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    cat $LOG_FILE
    exit 1
  fi

  # Ensure log is empty
  true >$LOG_FILE

  $UBOOT_DEV_DIR/tools/image_table_util                                       \
  --append --print --print-images                                             \
  --out "$DEV_BIN_PATH"                                                       \
  --name "DEV $GIT_STRING"                                                    \
  --timestamp $(date +%s)                                                     \
  --hardware v3_$HW_CONFIG                                                    \
  --image $UBOOT_DEV_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl        \
  --image $UBOOT_DEV_DIR/u-boot.img --image-type uboot                        \
  --image $OUTPUT_DIR/piksi_fpga.img --image-type fpga                        \
    | tee -a $LOG_FILE &>$LOG_DEBUG

  if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    cat $LOG_FILE
    exit 1
  fi

  rm $OUTPUT_DIR/piksi_fpga.img

  echo "done."
}

generate_prod()
{
  echo -n ">>> Generating PROD firmware image... "

  # Ensure log is empty
  true >$LOG_FILE

  $UBOOT_PROD_DIR/tools/image_table_util                                      \
  --append --print --print-images                                             \
  --out "$INTERNAL_BIN_PATH"                                                  \
  --name "$GIT_STRING"                                                        \
  --timestamp $(date +%s)                                                     \
  --hardware v3_$HW_CONFIG                                                    \
  --image $UBOOT_PROD_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl       \
  --image $UBOOT_PROD_DIR/u-boot.img --image-type uboot                       \
  --image $BINARIES_DIR/uImage.$CFG --image-type linux                        \
    | tee -a $LOG_FILE &>$LOG_DEBUG

  if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    cat $LOG_FILE
    exit 1
  fi

  echo "done."
}

generate_failsafe()
{
  [[ -z "${BR2_JUST_GEN_FAILSAFE:-}" ]] || local only=" (only)"

  echo -n ">>> Generating FAILSAFE firmware image${only:-}... "

  # Ensure log is empty
  true >$LOG_FILE

  $UBOOT_FAILSAFE_DIR/tools/image_table_util                                  \
  --append --print --print-images                                             \
  --out "$FAILSAFE_BIN_PATH"                                                  \
  --name "FSF $GIT_STRING"                                                    \
  --timestamp $(date +%s)                                                     \
  --hardware v3_$HW_CONFIG                                                    \
  --image $UBOOT_FAILSAFE_DIR/spl/u-boot-spl-dtb.img --image-type uboot-spl   \
  --image $UBOOT_FAILSAFE_DIR/u-boot.img --image-type uboot                   \
    | tee -a $LOG_FILE &>$LOG_DEBUG

  if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    cat $LOG_FILE
    exit 1
  fi

  if [[ -n "${BR2_JUST_GEN_FAILSAFE:-}" ]]; then
      echo ">>> Staging new failsafe bootloader..."
      mkdir -p $FIRMWARE_DIR
      cp -v $FAILSAFE_BIN_PATH $FIRMWARE_DIR/PiksiMulti-FAILSAFE.bin
  fi

  echo "done."
}

encrypt_and_sign()
{
  ${HOST_DIR}/usr/bin/encrypt_and_sign $INTERNAL_BIN_PATH $REL_PROT_BIN_PATH
  rm ${INTERNAL_BIN_PATH}
}

mkdir -p $OUTPUT_DIR
cp $UBOOT_PROD_DIR/tpl/boot.bin $OUTPUT_DIR

generate_failsafe

if [[ -n "${BR2_JUST_GEN_FAILSAFE:-}" ]]; then
  report_failsafe_loc
  exit 0
fi

if [[ -e $FIRMWARE_DIR/piksi_fpga.bit ]]; then
  generate_dev
else
  echo "*** WARNING: NO FPGA FIRMWARE FOUND, NOT BUILDING DEVELOPMENT IMAGE ***"
fi

cp $BINARIES_DIR/uImage.${CFG} $OUTPUT_DIR

if [[ -e $FIRMWARE_DIR/piksi_firmware.elf ]] && \
   [[ -e $FIRMWARE_DIR/piksi_fpga.bit ]]; then

  generate_prod

  if [[ -n "${BR2_BUILD_RELEASE_PROTECTED:-}" ]]; then
    encrypt_and_sign
  fi

  if [[ -n "${BR2_BUILD_RELEASE_OPEN:-}" ]]; then
      mv -v ${INTERNAL_BIN_PATH} ${REL_OPEN_BIN_PATH}
  fi

else
  echo "*** WARNING: NO FIRMWARE FILES FOUND, NOT BUILDING PRODUCTION IMAGE ***"
fi

# Strip absolute path in case we're inside the docker container
REL_OPEN_BIN_PATH=${REL_OPEN_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
REL_PROT_BIN_PATH=${REL_PROT_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
INTERNAL_BIN_PATH=${INTERNAL_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
DEV_BIN_PATH=${DEV_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
FAILSAFE_BIN_PATH=${FAILSAFE_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}

if [[ -f "${BR2_EXTERNAL_piksi_buildroot_PATH:-}/$INTERNAL_BIN_PATH" ]]; then
  echo -e "${bold}>>> INTERNAL firmware image located at:${normal}\n\t$INTERNAL_BIN_PATH"
fi

if [[ -n "${BR2_BUILD_RELEASE_OPEN:-}" ]]; then
  echo -e "${bold}>>> RELEASE/OPEN firmware image located at:${normal}\n\t$REL_OPEN_BIN_PATH"
fi

if [[ -n "${BR2_BUILD_RELEASE_PROTECTED:-}" ]]; then
  echo -e "${bold}>>> RELEASE/PROTECTED firmware image located at:${normal}\n\t$REL_PROT_BIN_PATH"
fi

echo -e "${bold}>>> DEV firmware image located at:${normal}\n\t$DEV_BIN_PATH"
report_failsafe_loc

echo
