#!/bin/bash

set -e

if [ -z "$HW_CONFIG" ]; then
  echo "ERROR: HW_CONFIG is not set"
  exit 1
fi

if [ -n "$DEBUG" ]; then
  LOG_DEBUG=/dev/stdout
else
  LOG_DEBUG=/dev/null
fi

BUILD_TOOLS_REPO=git@github.com:swift-nav/piksi_build_tools.git

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

UBOOT_BASE_DIR=`find $BUILD_DIR -maxdepth 1 -type d -name "uboot_custom-${UBOOT_VERSION}"`

DEV_BIN_PATH=$OUTPUT_DIR/PiksiMulti-DEV-$FILE_GIT_STRING.bin
FAILSAFE_BIN_PATH=$OUTPUT_DIR/PiksiMulti-FAILSAFE-$FILE_GIT_STRING.bin
INTERNAL_BIN_PATH=$OUTPUT_DIR/PiksiMulti-INTERNAL-$FILE_GIT_STRING.bin
REL_OPEN_BIN_PATH=$OUTPUT_DIR/PiksiMulti-$FILE_GIT_STRING.bin
REL_PROT_BIN_PATH=$OUTPUT_DIR/PiksiMulti-PROTECTED-$FILE_GIT_STRING.bin

if [[ -z "${UBOOT_BASE_DIR}" ]]; then
  echo "ERROR: Could not find uboot directory" >&2
  exit 1
fi

UBOOT_PROD_DIR=$UBOOT_BASE_DIR/build/${CFG}_prod
UBOOT_FAILSAFE_DIR=$UBOOT_BASE_DIR/build/${CFG}_failsafe
UBOOT_DEV_DIR=$UBOOT_BASE_DIR/build/${CFG}_dev

LOG_FILE=$(mktemp)
trap 'rm -rf $LOG_FILE' EXIT

generate_dev() {

  echo -n "Generating DEV firmware image... "

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

generate_prod() {

  echo -n "Generating PROD firmware image image... "

  if [ -n "$BR2_BUILD_RELEASE_PROTECTED" ] || [ -n "$BR2_BUILD_RELEASE_OPEN" ]; then
    echo "This is a release image, staging new failsafe bootloader..."
    cp -v $FAILSAFE_BIN_PATH $FIRMWARE_DIR/PiksiMulti-FAILSAFE.bin
  else
    echo "This is *NOT* a release image, purging stage of failsafe bootloader..."
    rm -vf $FIRMWARE_DIR/PiksiMulti-FAILSAFE.bin
  fi

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

generate_failsafe() {

  echo -n "Generating FAILSAFE firmware image image... "

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

  echo "done."
}

mkdir -p $OUTPUT_DIR

cp $UBOOT_PROD_DIR/tpl/boot.bin $OUTPUT_DIR
cp $BINARIES_DIR/uImage.${CFG} $OUTPUT_DIR

generate_failsafe

if [ -e $FIRMWARE_DIR/piksi_fpga.bit ]; then
  generate_dev
else
  echo "*** NO FPGA FIRMWARE FOUND, NOT BUILDING DEVELOPMENT IMAGE ***"
fi

if [ -e $FIRMWARE_DIR/piksi_firmware.elf ] && \
   [ -e $FIRMWARE_DIR/piksi_fpga.bit ]; then

  generate_prod

  if [ -n "$BR2_BUILD_RELEASE_PROTECTED" ]; then

    if ! git ls-remote ${BUILD_TOOLS_REPO} &>/dev/null; then
      echo "*** ERROR: No access to build tools repository, cannot create protected image ***"
    fi

    git clone ${BUILD_TOOLS_REPO} ${HOST_DIR}/piksi_build_tools
    ${HOST_DIR}/piksi_build_tools/bin/encrypt_and_sign $INTERNAL_BIN_PATH $REL_PROT_BIN_PATH

    rm ${INTERNAL_BIN_PATH}
    rm -rf ${HOST_DIR}/piksi_build_tools
  fi

  if [ -n "$BR2_BUILD_RELEASE_OPEN" ]; then
      mv -v ${INTERNAL_BIN_PATH} ${REL_OPEN_BIN_PATH}
  fi

else
  echo "*** NO FIRMWARE FILES FOUND, NOT BUILDING PRODUCTION IMAGE ***"
fi

# Strip absolute path in case we're inside the docker container
if [[ -n "$BR2_EXTERNAL_piksi_buildroot_PATH" ]]; then
  REL_OPEN_BIN_PATH=${REL_OPEN_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
  REL_PROT_BIN_PATH=${REL_PROT_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
  INTERNAL_BIN_PATH=${INTERNAL_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
  DEV_BIN_PATH=${DEV_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
  FAILSAFE_BIN_PATH=${FAILSAFE_BIN_PATH#${BR2_EXTERNAL_piksi_buildroot_PATH}/}
fi

bold=$(tput rev 2>/dev/null)
normal=$(tput sgr0 2>/dev/null)

if [ -f "${BR2_EXTERNAL_piksi_buildroot_PATH}/$INTERNAL_BIN_PATH" ]; then
  echo -e "${bold}>>> INTERNAL firmware image located at:${normal}\n\t$INTERNAL_BIN_PATH"
fi

if [ -n "$BR2_BUILD_RELEASE_OPEN" ]; then
  echo -e "${bold}>>> RELEASE/OPEN firmware image located at:${normal}\n\t$REL_OPEN_BIN_PATH"
fi

if [ -n "$BR2_BUILD_RELEASE_PROTECTED" ]; then
  echo -e "${bold}>>> RELEASE/PROTECTED firmware image located at:${normal}\n\t$REL_PROT_BIN_PATH"
fi

echo -e "${bold}>>> DEV firmware image located at:${normal}\n\t$DEV_BIN_PATH"
echo -e "${bold}>>> FAILSAFE firmware image located at:${normal}\n\t$FAILSAFE_BIN_PATH"
echo
