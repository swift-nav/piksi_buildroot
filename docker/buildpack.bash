#!/bin/bash

BR_BASE_VERSION=2017.10.30
BR2_EXTERNAL=/piksi_buildroot
BR2_DL_DIR=/piksi_buildroot/output/dl
O=/piksi_buildroot/output/target

HW_CONFIG="${HW_CONFIG:-prod}"

D=$( (cd "$(dirname "$0")" >/dev/null || exit 1; pwd -P) )

# shellcheck source=lib.bash
source "$D/lib.bash"

do_config_hash

BUILD_PHASE=0

declare -r BUILD_PHASE_CREATE=0
declare -r BUILD_PHASE_START=1
declare -r BUILD_PHASE_CONFIGURE=2
declare -r BUILD_PHASE_BUILD=3
declare -r BUILD_PHASE_STRIP=4
declare -r BUILD_PHASE_PACKAGE=5

while [[ $# -gt 0 ]]; do
  case $1 in
    --build-phase=start)
			BUILD_PHASE=$BUILD_PHASE_START
      shift
    ;;
    --build-phase=configure)
			BUILD_PHASE=$BUILD_PHASE_CONFIGURE
      shift
    ;;
    --build-phase=build)
			BUILD_PHASE=$BUILD_PHASE_BUILD
      shift
    ;;
    --build-phase=strip)
			BUILD_PHASE=$BUILD_PHASE_STRIP
      shift
    ;;
    --build-phase=package)
			BUILD_PHASE=$BUILD_PHASE_PACKAGE
      shift
		;;
    --build-phase=*)
			echo "Error: unknown build phase!" >&2
      shift
    ;;
  esac
done

echo "Will tag as '$BR_DOCKER_NAMETAG' (HW_CONFIG: $HW_CONFIG)..."

if [[ $BUILD_PHASE -le $BUILD_PHASE_CREATE ]]; then

	echo ">>> Phase: create"

	docker stop piksi_prebuild &>/dev/null || echo "piksi_prebuild not running"
	docker rm piksi_prebuild &>/dev/null   || echo "piksi_prebuild did not exist already"
fi

if [[ $BUILD_PHASE -le $BUILD_PHASE_START ]]; then

	echo ">>> Phase: start"

	docker create -i -t \
		-e HW_CONFIG="$HW_CONFIG" \
		-e BR2_EXTERNAL=$BR2_EXTERNAL \
		-e BR2_DL_DIR=$BR2_DL_DIR \
		-v "$PWD:/piksi_buildroot" \
		-v piksi_prebuild-output:/piksi_buildroot/output \
		--name piksi_prebuild \
		"swiftnav/buildroot-base:$BR_BASE_VERSION" \
		/bin/bash

	docker start piksi_prebuild
fi

if [[ $BUILD_PHASE -le $BUILD_PHASE_CONFIGURE ]]; then

	echo ">>> Phase: configure"

	docker exec -it piksi_prebuild bash -c "cd /piksi_buildroot;
		export HW_CONFIG=$HW_CONFIG BR2_EXTERNAL=$BR2_EXTERNAL BR2_DL_DIR=$BR2_DL_DIR;
		make -C buildroot O=${O} piksiv3_defconfig"
fi

if [[ $BUILD_PHASE -le $BUILD_PHASE_BUILD ]]; then

	echo ">>> Phase: build"

	docker exec -it piksi_prebuild bash -c "cd /piksi_buildroot;
		export HW_CONFIG=$HW_CONFIG BR2_EXTERNAL=$BR2_EXTERNAL BR2_DL_DIR=$BR2_DL_DIR;
		make -C buildroot O=${O} all"
fi

if [[ $BUILD_PHASE -le $BUILD_PHASE_STRIP ]]; then

	echo ">>> Phase: strip"

	# shellcheck disable=SC2016,SC1004
	docker exec -it piksi_prebuild bash -c "cd /piksi_buildroot;
			export HW_CONFIG=$HW_CONFIG BR2_EXTERNAL=$BR2_EXTERNAL BR2_DL_DIR=$BR2_DL_DIR;
			find package -mindepth 1 -maxdepth 1 -type d | grep -v uboot_custom | sed s@package/@@ \
				| while read x; do echo \$x-dirclean; done \
				| xargs make -C buildroot O=${O}"

	docker exec -it piksi_prebuild bash -c 'cd /piksi_buildroot;
		if [[ -d output/target/target/lib/firmware ]]; then
			find output/target/target/lib/firmware -print -delete;
		fi'
fi

if [[ $BUILD_PHASE -le $BUILD_PHASE_PACKAGE ]]; then

	echo ">>> Phase: package"

	docker cp "$D/Dockerfile.buildpack" piksi_prebuild:/piksi_buildroot/Dockerfile.bp

	docker build \
		--no-cache \
		--force-rm \
		-f Dockerfile.bp \
		--tag "$BR_DOCKER_NAMETAG" \
		- < <( docker exec piksi_prebuild \
						tar -C /piksi_buildroot -czf - Dockerfile.bp output buildroot )

	echo "Tagged as '$BR_DOCKER_NAMETAG'."
fi
