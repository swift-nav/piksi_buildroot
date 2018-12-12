#!/usr/bin/env bash

BOARD_DIR="$(dirname $0)"

# copy the uEnv.txt to the output/images directory
cp ${BOARD_DIR}/uEnv.txt ${BINARIES_DIR}/uEnv.txt
zcat ${BOARD_DIR}/empty.ext4.gz > ${BINARIES_DIR}/empty.ext4

GENIMAGE_CFG="${BOARD_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

rm -rf "${GENIMAGE_TMP}"

genimage \
	--rootpath "${TARGET_DIR}" \
	--tmppath "${GENIMAGE_TMP}" \
	--inputpath "${BINARIES_DIR}" \
	--outputpath "${BINARIES_DIR}" \
	--config "${GENIMAGE_CFG}"
