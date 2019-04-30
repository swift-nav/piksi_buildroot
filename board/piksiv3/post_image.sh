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

if [[ -z "${VARIANT:-}" ]]; then
  echo "ERROR: the variable 'VARIANT' cannot be empty"
fi

SCRIPTS="${BR2_EXTERNAL_piksi_buildroot_PATH}/scripts"

bold=$(tput rev 2>/dev/null || :)
normal=$(tput sgr0 2>/dev/null || :)

GIT_STRING=$("$(dirname "$0")"/get_git_string.sh)
# remove the githash from the filename for useability
FILE_GIT_STRING=${GIT_STRING%%-g*}

CFG=piksiv3_$HW_CONFIG
OUTPUT_DIR=$BINARIES_DIR/${CFG}
FIRMWARE_DIR=$TARGET_DIR/lib/firmware

UBOOT_MK_PATH="$BR2_EXTERNAL_piksi_buildroot_PATH/package/uboot_custom/uboot_custom.mk"

UBOOT_VERSION_REGEX='UBOOT_CUSTOM_VERSION *= *'
UBOOT_VERSION=$(grep "$UBOOT_VERSION_REGEX" $UBOOT_MK_PATH | sed "s/${UBOOT_VERSION_REGEX}\\(.*\\)/\\1/")

UBOOT_BASE_DIR=$(find $BUILD_DIR -maxdepth 1 -type d -name "uboot_custom-${UBOOT_VERSION}")

SDK_FPGA_SHA1="${BR2_EXTERNAL_piksi_buildroot_PATH}/firmware/prod/piksi_sdk_fpga.sha1sum"

DEV_BIN_PATH=$OUTPUT_DIR/PiksiMulti-DEV-$FILE_GIT_STRING.bin
FAILSAFE_BIN_PATH=$OUTPUT_DIR/PiksiMulti-FAILSAFE-$FILE_GIT_STRING.bin

IMAGE_NAME=$("$SCRIPTS/get-variant-prop" "$VARIANT" image_name version=$FILE_GIT_STRING)
IMAGE_BIN_PATH=$OUTPUT_DIR/$IMAGE_NAME

INTERNAL_BIN_PATH=$OUTPUT_DIR/PiksiMulti-INTERNAL-$FILE_GIT_STRING.bin
BUILD_TYPE_DETECTED=INTERNAL

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
  --out "$IMAGE_BIN_PATH"                                                  \
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

  echo ">>> Starting FAILSAFE firmware image${only:-} generation..."

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
    echo "ERROR: generating failsafe image:"
    cat $LOG_FILE
    exit 1
  fi

  if [[ -n "${BR2_JUST_GEN_FAILSAFE:-}" ]]; then
      echo ">>> Staging new failsafe bootloader..."
      mkdir -p $FIRMWARE_DIR
      cp -v $FAILSAFE_BIN_PATH $FIRMWARE_DIR/PiksiMulti-FAILSAFE.bin
  fi

  echo ">>> Done generating FAILSAFE firmware image${only:-}..."
}

encrypt_and_sign()
{
  local eas_tool_path=${HOST_DIR}/usr/bin/encrypt_and_sign
  if [[ ! -f "$eas_tool_path" ]]; then
    echo "ERROR: 'encrypt_and_sign' tool not found at path: $eas_tool_path" >&2
    exit 1
  fi
  "$eas_tool_path" "$IMAGE_BIN_PATH" "${IMAGE_BIN_PATH}.enc"
  mv "${IMAGE_BIN_PATH}.enc" "$IMAGE_BIN_PATH"
}

rename_if_sdk_build()
{
  local eas_tool_path=${HOST_DIR}/usr/bin/encrypt_and_sign
  if [[ -e "$IMAGE_BIN_PATH" ]]; then
    if [[ -e $SDK_FPGA_SHA1 ]]; then
      if [[ -f "$eas_tool_path" ]]; then
        echo "ERROR: found 'encrypt_and_sign', for \"SDK\" builds, this tool not *SHOULD NOT BE* locatable ($eas_tool_path)" >&2
        exit 1
      fi
      local sha=$(sha1sum "$FIRMWARE_DIR/piksi_fpga.bit" | cut -d' ' -f1)
      if [[ "$sha" == "$(cat $SDK_FPGA_SHA1 | cut -d' ' -f1)" ]]; then
        echo '>>> SDK build detected....'
        BUILD_TYPE_DETECTED=SDK
      fi
    fi
  fi
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

BR2_BUILD_RELEASE_OPEN=
BR2_BUILD_RELEASE_PROTECTED=

if [[ "$("$SCRIPTS/get-variant-prop" "$VARIANT" encrypted)" == True ]]; then
  BR2_BUILD_RELEASE_PROTECTED=y
fi

if [[ "$VARIANT" == "unprotected" ]]; then
  BR2_BUILD_RELEASE_OPEN=y
fi

if [[ "$VARIANT" == "base" ]]; then
  BUILD_TYPE_DETECTED=BASE
fi

if [[ -e $FIRMWARE_DIR/piksi_firmware.elf ]] && \
   [[ -e $FIRMWARE_DIR/piksi_fpga.bit ]]; then

  generate_prod

  if [[ -n "${BR2_BUILD_RELEASE_PROTECTED:-}" ]]; then
    echo '>>> Protected build detected...'
    BUILD_TYPE_DETECTED=PROTECTED

    encrypt_and_sign
  fi

  if [[ -n "${BR2_BUILD_RELEASE_OPEN:-}" ]]; then
    echo '>>> Unprotected build detected...'
    BUILD_TYPE_DETECTED=UNPROTECTED
  fi

  rename_if_sdk_build

else
  echo "*** WARNING: NO FIRMWARE FILES FOUND, NOT BUILDING PRODUCTION IMAGE ***"
fi

# Strip absolute path in case we're inside the docker container
IMAGE_BIN_PATH=${IMAGE_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
DEV_BIN_PATH=${DEV_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
FAILSAFE_BIN_PATH=${FAILSAFE_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}

if [[ -f "${BR2_EXTERNAL_piksi_buildroot_PATH:-}/$IMAGE_BIN_PATH" ]]; then
  echo -e "${bold}>>> $BUILD_TYPE_DETECTED firmware image located at:${normal}\n\t$IMAGE_BIN_PATH"
fi

echo -e "${bold}>>> DEV firmware image located at:${normal}\n\t$DEV_BIN_PATH"
report_failsafe_loc

echo
