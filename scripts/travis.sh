#!/usr/bin/env bash

set -e -o pipefail

PHASE=$1; shift

#######################################################################
# Global state ########################################################
#######################################################################

BUILD_LOG=build_${TRAVIS_TARGET}.out
FILE=$(basename $0)

#######################################################################
# Library code ########################################################
#######################################################################

die_error()
{
  local err=$1; shift

  echo "ERROR: ${err} (${BASH_SOURCE[1]##*/}:${FUNCNAME[1]}[${BASH_LINENO[0]}])" >&2
  exit 1
}

validate_travis_target()
{
  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
    :
  elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
    :
  elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
    :
  elif [[ "${TRAVIS_TARGET}" == "toolchain" ]]; then
    :
  elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
    :
  elif [[ "${TRAVIS_TARGET}" == "nano" ]]; then
    :
  elif [[ "${TRAVIS_TARGET}" == "format" ]]; then
    :
  elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
    :
  else
    die_error "unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}"
  fi
}

ticker_pid=
trap '( [[ -z "${ticker_pid}" ]] || kill "${ticker_pid}" || : ) &>/dev/null' EXIT

SLEEP_TIME=60

spawn_ticker()
{
  ( set +e; while true; do sleep $SLEEP_TIME; echo ...; done ) &
  ticker_pid=$!
}

kill_ticker()
{
  ( [[ -z "${ticker_pid}" ]] || kill "${ticker_pid}" || : ) &>/dev/null
}

capture_build_log()
{
  tee -a $BUILD_LOG | grep --line-buffered '^>>>'
}

## Description: trigger other systems required while processing a PR build
##   the scripts here will typical touch things like HITL (test automation),
##   Slack and GitHub.
trigger_external_systems()
{
  # Generate data required by HITL and kick-off a smoke test run
  ./scripts/travis_pr_describe.sh
  ./scripts/generate_requirements.sh
  ./scripts/hitl_smoke_test.sh

  # Push a comment to Slack #github channel
  SLACK_CHANNEL=github \
    ./scripts/comment.sh
}

## Description: List all files that will be published by various build phases,
##   some build phases (like the SDK/ccache build) will manually publish
##   additional files depending on environmental conditions (such as tag
##   states).
list_published_files()
{
  local files=$BUILD_LOG

  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
    # Only push "release" (locked down, encrypted, includes INS), and the 
    #   unencrypted/unprotected image (locked down, not encrypted, and does
    #   not include INS support).
    files="${files} \
      buildroot/output/images/piksiv3_prod/PiksiMulti-v*.bin \
      buildroot/output/images/piksiv3_prod/PiksiMulti-UNPROTECTED-v*.bin"
  elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
    : # Just push build log
  elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
    # Push all images (failsafe, internal, dev)
    files="${files} \
         buildroot/output/images/piksiv3_prod/*"
  elif [[ "${TRAVIS_TARGET}" == "toolchain" ]]; then
    : # Just push build log
  elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
    : # Just push build log
  elif [[ "${TRAVIS_TARGET}" == "nano" ]]; then
    files="${files} \
      buildroot/nano_output/images/sdcard.img"
  elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
    files="${files} \
      buildroot/output/images/piksiv3_prod/PiksiMulti-SDK-v*.bin"
  else
    die_error "unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}"
  fi

  echo "$files"
}

do_default_after_failure_actions()
{
  tail -n 500 $BUILD_LOG

  PRODUCT_VERSION=v3 PRODUCT_REV=prod \
    ./scripts/publish.sh $BUILD_LOG
}

check_format_errors() {
  if [[ $(git --no-pager diff --name-only HEAD) ]]; then
    echo "######################################################"
    echo "####### clang-format warning found! Exiting... #######"
    echo "######################################################"
    echo ""
    echo "This should be formatted locally and pushed again..."
    git --no-pager diff
    exit 1
  fi
}

#######################################################################
# Docker build variant ################################################
#######################################################################

handle_docker_script_phase()
{
  spawn_ticker
  ./scripts/docker_travis.sh 2>&1 | capture_build_log
}

handle_docker_after_success_phase()
{
  PRODUCT_VERSION=v3 PRODUCT_REV=prod \
    ./scripts/publish.sh $BUILD_LOG
}

handle_docker_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# Internal build variant ##############################################
#######################################################################

handle_internal_script_phase()
{
  export CCACHE_READONLY=1

  make docker-setup
  make docker-pull-ccache
  make docker-make-firmware

  spawn_ticker

  make docker-make-image 2>&1 | capture_build_log
}

handle_internal_after_success_phase()
{
  PRODUCT_VERSION=v3 PRODUCT_REV=prod \
    ./scripts/publish.sh $(list_published_files)

  trigger_external_systems
}

handle_internal_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# Release build variant ###############################################
#######################################################################

handle_release_script_phase()
{
  export CCACHE_READONLY=1

  make docker-setup
  make docker-make-firmware
  make docker-pull-ccache

  spawn_ticker

  make docker-make-image-release-open 2>&1 | capture_build_log
  make docker-make-image-release-ins 2>&1 | capture_build_log
}

handle_release_after_success_phase()
{
  PRODUCT_VERSION=v3 PRODUCT_REV=prod \
    ./scripts/publish.sh $(list_published_files)
}

handle_release_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# Host build variant ##################################################
#######################################################################

handle_host_script_phase()
{
  make docker-setup

  if ! ./scripts/need_tag_artifacts.sh; then
    make docker-host-pull-ccache
    export CCACHE_READONLY=1
  fi

  spawn_ticker
  make docker-make-host-image 2>&1 | capture_build_log
}

ccache_variant()
{
  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
    echo "release"
  elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
    echo "release"
  elif [[ "${TRAVIS_TARGET}" == "toolchain" ]]; then
    echo "release"
  elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
    echo "host"
  elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
    echo "release"
  elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
    die_error "invalid build variant for ccache: ${TRAVIS_TARGET}"
  elif [[ "${TRAVIS_TARGET}" == "nano" ]]; then
    die_error "invalid build variant for ccache: ${TRAVIS_TARGET}"
  elif [[ "${TRAVIS_TARGET}" == "format" ]]; then
    die_error "invalid build variant for ccache: ${TRAVIS_TARGET}"
  else
    die_error "unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}"
  fi
}

handle_after_success_phase()
{
  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
    handle_release_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
    handle_docker_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
    handle_internal_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "toolchain" ]]; then
    handle_toolchain_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
    handle_host_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "nano" ]]; then
    handle_nano_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "format" ]]; then
    handle_format_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
    handle_sdk_after_success_phase
  else
    die_error "unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}"
  fi
}

handle_host_after_success_phase()
{
  git fetch --tags --unshallow

  PRODUCT_VERSION=v3 PRODUCT_REV=prod \
    ./scripts/publish.sh $(list_published_files)

  if ./scripts/need_tag_artifacts.sh; then
    make host-ccache-archive
    ./scripts/publish.sh piksi_br_$(ccache_variant)_ccache.tgz
  fi
}

handle_host_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# Nano/Starling-EVK build variant #####################################
#######################################################################

handle_nano_script_phase()
{
  export CCACHE_READONLY=1

  make docker-setup
  make docker-pull-ccache

  spawn_ticker

  make docker-make-nano-image 2>&1 | capture_build_log
}

handle_nano_after_success_phase()
{
  # Make buildroot ignore our output directory so we aren't flagged as dirty
  echo "nano_output" >> buildroot/.gitignore
  (cd buildroot; git update-index --assume-unchanged .gitignore)

  PRODUCT_VERSION=nano PRODUCT_REV=evt0 \
    ./scripts/publish.sh $(list_published_files)
}

handle_nano_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# Toolchain build variant #############################################
#######################################################################

handle_toolchain_script_phase()
{
  make docker-setup
  make docker-make-firmware

  if ! ./scripts/need_tag_artifacts.sh; then
    make docker-pull-ccache
    export CCACHE_READONLY=1
  fi

  spawn_ticker

  export BR2_BUILD_SAMPLE_DAEMON=y
  export BR2_BUILD_PIKSI_INS_REF=y

  make docker-make-image-sdk 2>&1 | capture_build_log

  if ./scripts/need_tag_artifacts.sh; then
    make docker-export-toolchain 2>&1 | capture_build_log
  fi
}

handle_toolchain_after_success_phase()
{
  PRODUCT_VERSION=v3 PRODUCT_REV=prod \
    ./scripts/publish.sh $(list_published_files)

  if ./scripts/need_tag_artifacts.sh; then

    PRODUCT_VERSION=v3 PRODUCT_REV=prod \
      ./scripts/publish.sh piksi_br_toolchain.txz

    make docker-ccache-archive
    ./scripts/publish.sh piksi_br_$(ccache_variant)_ccache.tgz
  fi
}

handle_toolchain_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# Format build variant ################################################
#######################################################################

handle_format_script_phase()
{
  make clang-format && check_format_errors
}

handle_format_after_success_phase()
{
  :
}

handle_format_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# SDK build variant ###################################################
#######################################################################

should_build_sdk_image()
{
  if [[ -n "${FORCE_SDK_BUILD:-}" ]]; then
    return 0
  fi

  if ./scripts/need_tag_artifacts.sh; then
    return 0
  fi

  return 1
}

handle_sdk_script_phase()
{
  set -x

  if ! should_build_sdk_image; then
    echo '>>> Not building SDK image (not a tagged build or not requested)...'
    return
  fi

  spawn_ticker

  local tag=piksi_br_sdk_build
  local build_dir=$(mktemp -d)
  local branch_name=$(git branch --list | head -1 | sed s@^..@@)

  cp -v scripts/Dockerfile.sdk "$build_dir/Dockerfile"

  pushd $build_dir &>/dev/null

  echo '>>> Running fully self-contained docker build...'

  docker build \
    --build-arg branch=$branch_name \
    --tag $tag \
    .

  echo '>>> Copying artifacts for SDK image build...'

  docker run --name ${tag}-run --rm -it ls -l buildroot/output/images
  docker run -v $PWD:/output --name ${tag}-run --rm cp -vr buildroot/output/images/ /output/

  popd &>/dev/null

  kill_ticker
}

handle_sdk_after_success_phase()
{
  if ! should_build_sdk_image; then
    return
  fi
 
  PRODUCT_VERSION=v3 PRODUCT_REV=prod \
    ./scripts/publish.sh $(list_published_files)
}

handle_sdk_after_failure_phase()
{
  do_default_after_failure_actions
}

#######################################################################
# Travis build phase handling #########################################
#######################################################################

handle_script_phase()
{
  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
    handle_release_script_phase
  elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
    handle_docker_script_phase
  elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
    handle_internal_script_phase
  elif [[ "${TRAVIS_TARGET}" == "toolchain" ]]; then
    handle_toolchain_script_phase
  elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
    handle_host_script_phase
  elif [[ "${TRAVIS_TARGET}" == "nano" ]]; then
    handle_nano_script_phase
  elif [[ "${TRAVIS_TARGET}" == "format" ]]; then
    handle_format_script_phase
  elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
    handle_sdk_script_phase
  else
    die_error "unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}"
  fi
}

handle_after_success_phase()
{
  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
    handle_release_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
    handle_docker_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
    handle_internal_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "toolchain" ]]; then
    handle_toolchain_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
    handle_host_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "nano" ]]; then
    handle_nano_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "format" ]]; then
    handle_format_after_success_phase
  elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
    handle_sdk_after_success_phase
  else
    die_error "unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}"
  fi
}

handle_after_failure_phase()
{
  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
    handle_release_after_failure_phase
  elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
    handle_docker_after_failure_phase
  elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
    handle_internal_after_failure_phase
  elif [[ "${TRAVIS_TARGET}" == "toolchain" ]]; then
    handle_toolchain_after_failure_phase
  elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
    handle_host_after_failure_phase
  elif [[ "${TRAVIS_TARGET}" == "nano" ]]; then
    handle_nano_after_failure_phase
  elif [[ "${TRAVIS_TARGET}" == "format" ]]; then
    handle_format_after_failure_phase
  elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
    handle_sdk_after_failure_phase
  else
    die_error "unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}"
  fi
}

handle_before_install_phase()
{
  ./scripts/check_for_s3_cred.sh || exit 1

   openssl aes-256-cbc \
    -K $encrypted_09ba210c188e_key \
    -iv $encrypted_09ba210c188e_iv \
    -in .travis_piksi_ins_ref_key.enc \
    -out /tmp/travis_piksi_ins_ref_key \
    -d

   chmod 0600 /tmp/travis_piksi_ins_ref_key
   ssh-add /tmp/travis_piksi_ins_ref_key

   pip install --user --upgrade awscli

   git fetch --tags --unshallow

   sudo apt-get update
   sudo apt-get -y -o Dpkg::Options::="--force-confnew" install docker-ce
}

handle_after_script_phase()
{
  rm /tmp/travis_piksi_ins_ref_key
}

validate_travis_target

if [[ "$PHASE" == "script" ]]; then
  handle_script_phase
elif [[ "$PHASE" == "after_success" ]]; then
  handle_after_success_phase || exit 1
elif [[ "$PHASE" == "after_failure" ]]; then
  handle_after_failure_phase
elif [[ "$PHASE" == "before_install" ]]; then
  handle_before_install_phase
elif [[ "$PHASE" == "after_script" ]]; then
  handle_after_script_phase
else
  echo "ERROR: unknown build phase" >&2
  exit 1
fi

# vim: expandtab:shiftwidth=2:softtabstop=2:tabstop=2:
