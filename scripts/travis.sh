#!/usr/bin/env bash

set -e -x -o pipefail

PHASE=$1; shift

#######################################################################
# Global state ########################################################
#######################################################################

BUILD_LOG=build_${TRAVIS_TARGET}.out

#######################################################################
# Library code ########################################################
#######################################################################

validate_travis_target()
{
  if [[ "${TRAVIS_TARGET}" == "release" ]]; then
		:
	elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
		:
	elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
		:
	elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
		:
	elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
		:
	else
		echo "ERROR: unknown TRAVIS_TARGET value: ${TRAVIS_TARGET}" >&2
		exit 1
	fi
}

ticker_pid=
trap '[[ -z "${ticker_pid}" ]] || kill "${ticker_pid}"' EXIT TERM INT

spawn_ticker()
{
	( while true; do sleep 1m; echo ...; done ) &
	ticker_pid=$!
}

capture_build_log()
{
	tee -a  | grep --line-buffered '^>>>'
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
		# Only push "release" (not locked down), and the encrypted image (which
		#   should include INS support).
		files="${files} \
			buildroot/output/images/piksiv3_prod/PiksiMulti-v*.bin \
			buildroot/output/images/piksiv3_prod/PiksiMulti-PROTECTED-v*.bin"
	elif [[ "${TRAVIS_TARGET}" == "docker" ]]; then
		: # Just push build log
	elif [[ "${TRAVIS_TARGET}" == "internal" ]]; then
		# Push all images (failsafe, internal, dev)
		files="${files} \
 				buildroot/output/images/piksiv3_prod/*"
	elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
		: # Just push build log
	elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
		: # Just push build log
	fi

	echo "$files"
}

do_default_after_failure_actions()
{
	tail -n 500 $BUILD_LOG

	PRODUCT_VERSION=v3 PRODUCT_REV=prod \
		./publish.sh $BUILD_LOG
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
		./publish.sh $BUILD_LOG
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
	make docker-setup
	make docker-pull-ccache
	make docker-make-firmware

	spawn_ticker

	make docker-make-image 2>&1 | capture_build_log
}

handle_internal_after_success_phase()
{
	PRODUCT_VERSION=v3 PRODUCT_REV=prod \
		./publish.sh $(list_published_files)

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
		./publish.sh $(list_published_files)
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

	if ! ./scripts/should_build_sdk_and_ccache.sh; then
		make docker-host-pull-ccache
	fi

	spawn_ticker
	make docker-make-host-image 2>&1 | capture_build_log
}

handle_host_after_success_phase()
{
	git fetch --tags --unshallow

	PRODUCT_VERSION=v3 PRODUCT_REV=prod \
		./publish.sh $(list_published_files)

	if ./scripts/should_build_sdk_and_ccache.sh; then
		make host-ccache-archive
		./publish.sh piksi_br_${TRAVIS_TARGET}_ccache.tgz
	fi
}

handle_host_after_failure_phase()
{
	do_default_after_failure_actions
}

#######################################################################
# SDK build variant ###################################################
#######################################################################

handle_sdk_script_phase()
{
	make docker-setup
	make docker-make-firmware

	# Don't need to pull ccache because in case where we build we're pushing
  #   the SDK and the cache.

	spawn_ticker
	make docker-make-image 2>&1 | capture_build_log

	if ./scripts/should_build_sdk_and_ccache.sh; then
		make docker-make-sdk 2>&1 | capture_build_log
	fi
}

handle_sdk_after_success_phase()
{
	PRODUCT_VERSION=v3 PRODUCT_REV=prod \
		./publish.sh $(list_published_files)

	if ./scripts/should_build_sdk_and_ccache.sh; then

		PRODUCT_VERSION=v3 PRODUCT_REV=prod \
			./publish.sh piksi_sdk.txz

		make docker-ccache-archive
		./publish.sh piksi_br_${TRAVIS_TARGET}_ccache.tgz
	fi
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
	elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
		handle_sdk_script_phase
	elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
		handle_host_script_phase
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
	elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
		handle_sdk_after_success_phase
	elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
		handle_host_after_success_phase
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
	elif [[ "${TRAVIS_TARGET}" == "sdk" ]]; then
		handle_sdk_after_failure_phase
	elif [[ "${TRAVIS_TARGET}" == "host" ]]; then
		handle_host_after_failure_phase
	fi
}

handle_before_install_phase()
{
	./scripts/check_for_s3_cred.sh || travis_terminate 1

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
	handle_after_success_phase || travis_terminate 1
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
