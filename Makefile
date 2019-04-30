include scripts/set-shell.mk
include scripts/env-setup.mk

BR2_EXTERNAL := $(CURDIR)

ifeq ($(HW_CONFIG),)
HW_CONFIG    := prod
endif

.PHONY: all config image clean                                  \
        docker-setup docker-make-image docker-make-clean        \
        docker-config docker-run docker-shell                   \
        docker-config pkg-% docker-pkg-%                        \
        docker-rebuild-changed rebuild-changed _rebuild_changed

all: image

include scripts/docker.mk

external-artifacts:
	@./scripts/fetch-external-artifacts $(VARIANT)

config:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=$(VARIANT_OUTPUT) VARIANT=$(VARIANT) $(VARIANT_CONFIG)

pre-flight-checks:
	@./scripts/run-preflight-checks $(VARIANT)

image: pre-flight-checks external-artifacts config
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=$(VARIANT_OUTPUT) VARIANT=$(VARIANT) V=$(V)

clean-ccache:
	rm -rf buildroot/output/ccache/*

clean:
	find buildroot/$(VARIANT_OUTPUT) -mindepth 1 -maxdepth 1 \
		\( ! -path buildroot/$(VARIANT_OUTPUT)/images \) \
		-print -exec rm -rf {} \;
	rm -rf buildroot/$(VARIANT_OUTPUT)/images/*

flush-rootfs:
	find buildroot/$(VARIANT_OUTPUT)/target -name .stamp_target_installed -delete
	rm -rf buildroot/$(VARIANT_OUTPUT)/target/*

# 'Package-specific:'
# '  pkg-<pkg>                  - Build and install <pkg> and all its dependencies'	
# '  pkg-<pkg>-source           - Only download the source files for <pkg>'	
# '  pkg-<pkg>-extract          - Extract <pkg> sources'	
# '  pkg-<pkg>-patch            - Apply patches to <pkg>'	
# '  pkg-<pkg>-depends          - Build <pkg>'\''s dependencies'	
# '  pkg-<pkg>-configure        - Build <pkg> up to the configure step'	
# '  pkg-<pkg>-build            - Build <pkg> up to the build step'	
# '  pkg-<pkg>-dirclean         - Remove <pkg> build directory'	
# '  pkg-<pkg>-reconfigure      - Restart the build from the configure step'	
# '  pkg-<pkg>-rebuild          - Restart the build from the build step'
pkg-%: config
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot $* O=$(VARIANT_OUTPUT) VARIANT=$(VARIANT)

rebuild-changed: export BUILD_TEMP=/tmp SINCE=$(SINCE) IGNORE=$(_REBUILD_CHANGED_IGNORE)
rebuild-changed: _rebuild_changed

_REBUILD_CHANGED_IGNORE_DEFAULT := release_lockdown,piksi_ins,sample_daemon,llvm_o,llvm_v,build_tools

ifneq ($(IGNORE),)
_REBUILD_CHANGED_IGNORE := $(IGNORE),$(_REBUILD_CHANGED_IGNORE_DEFAULT)
else
_REBUILD_CHANGED_IGNORE := $(_REBUILD_CHANGED_IGNORE_DEFAULT)
endif

_rebuild_changed:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot \
			$(shell BUILD_TEMP=$(BUILD_TEMP) SINCE=$(SINCE) IGNORE=$(IGNORE) \
				scripts/changed_project_targets.py) \
			O=$(VARIANT_OUTPUT) VARIANT=$(VARIANT)

_print_db:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot all O=$(VARIANT_OUTPUT) VARIANT=$(VARIANT) -np

docker-build-image:
	docker build --no-cache --force-rm \
		--build-arg VERSION_TAG=$(shell cat scripts/docker_version_tag) \
		--build-arg USER=$(SANITIZED_USER) \
		--build-arg UID=$(UID) \
		--build-arg GID=$(GID) \
		--tag $(DOCKER_TAG) -f scripts/Dockerfile .

docker-populate-volume:
	docker run $(DOCKER_SETUP_ARGS) $(DOCKER_TAG) \
		git submodule update --init --recursive

docker-setup: docker-build-image docker-populate-volume

docker-rebuild-changed:
	docker run \
		$(DOCKER_ARGS) \
		-e BUILD_TEMP=/host/tmp \
		-e SINCE=$(SINCE) \
		-e IGNORE=$(_REBUILD_CHANGED_IGNORE) \
		$(DOCKER_TAG) \
		make _rebuild_changed

docker-make-image:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make image

docker-make-clean:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make clean

docker-make-clean-ccache:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make clean-ccache

docker-make-clean-volume:
	docker volume rm $(DOCKER_BUILD_VOLUME)

docker-make-clean-all: docker-make-clean docker-make-clean-volume

docker-config:
	docker run $(DOCKER_RUN_ARGS) $(DOCKER_TAG) \
		make -C buildroot O=$(VARIANT_OUTPUT) VARIANT=$(VARIANT) $(VARIANT_CONFIG)

docker-pkg-%: docker-config
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make -C buildroot $* O=$(VARIANT_OUTPUT) VARIANT=$(VARIANT)

docker-shell docker-run:
	docker run $(DOCKER_RUN_ARGS) --name=$(DOCKER_TAG) \
		--tty --interactive $(DOCKER_TAG) $(ARGS) || :

docker-exec:
	docker exec $(DOCKER_ENV_ARGS) --interactive --tty \
		$(DOCKER_TAG) /bin/bash -s 'export PS1="\u@\h:\w\$$"'

docker-make-firmware:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make firmware

docker-make-flush-rootfs:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make flush-rootfs

docker-cp:
	docker run $(DOCKER_RUN_ARGS) --name=$(DOCKER_TAG)-copy -d $(DOCKER_TAG)
	docker cp $(DOCKER_TAG)-copy:$(SRC) $(DST) || :
	docker stop $(DOCKER_TAG)-copy

docker-jenkins:
	docker-compose --file scripts/docker-compose.yml --project-directory ${PWD} run pbr

help:
	@[[ -x $(shell which less) ]] && \
		less $(CURDIR)/scripts/make_help.txt || \
		cat $(CURDIR)/scripts/make_help.txt

clang-complete-config:
	@./scripts/gen-clang-complete

docker-clang-complete-config:
	@docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		./scripts/gen-clang-complete

run-clang-tidy:
	@./scripts/run-clang-tidy $(ARGS)

docker-run-clang-tidy:
	@docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		./scripts/run-clang-tidy $(ARGS)

run-clang:
	@./scripts/run-clang $(ARGS)

docker-run-clang:
	docker run $(DOCKER_ARGS) -e CLANG_STDIN=$(CLANG_STDIN) $(DOCKER_TAG) \
		./scripts/run-clang $(ARGS)

clang-format:
	@./scripts/run-clang-format

docker-clang-format:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		./scripts/run-clang-format

docker-make-export-toolchain:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make export-toolchain

export-toolchain:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=$(VARIANT_OUTPUT) V=$(V) sdk
	@echo '>>>' Uninstalling piksi toolchain wrappers...
	$(MAKE) -C buildroot force-uninstall-toolchain-wrappers
	@echo '>>>' Creating buildroot toolchain archive...
	tar -cJf piksi_br_toolchain.txz -C buildroot/$(VARIANT_OUTPUT)/host .

ccache-archive:
	tar -C buildroot/ccache -czf piksi_br_ccache.tgz .

docker-ccache-archive:
	docker run $(DOCKER_RUN_ARGS) $(DOCKER_TAG) \
		make ccache-archive

docker-sync-setup:
	@./scripts/check-docker-sync
	@./scripts/gen-docker-sync $(DOCKER_BUILD_VOLUME) $(UID) $(DOCKER_HOST)
	@docker volume create --name=$(DOCKER_BUILD_VOLUME)-sync
	@echo "Done, run: make docker-sync-start"

docker-sync-logs:
	@docker-sync logs -c .docker-sync.yml

docker-sync-start:
	@docker-sync start -c .docker-sync.yml

docker-sync-stop:
	@docker-sync stop -c .docker-sync.yml

docker-sync-restart:
	@docker-sync stop -c .docker-sync.yml && docker-sync start -c .docker-sync.yml

docker-sync-clean:
	@docker-sync clean -c .docker-sync.yml || echo "docker-sync clean failed..."
	@rm -f .docker-compose.yml .docker-sync.yml 

docker-sync-wait:
	@echo -n "Waiting for docker-sync..."
	@date >.check-docker-sync
	@while [[ "$$(docker run $(DOCKER_RUN_ARGS) $(DOCKER_TAG) cat .check-docker-sync | tr -d $$'\r')" != \
		  "$$(cat .check-docker-sync)" ]]; do sleep 0.5; done
	@echo " done."

docker-sync-check:
	@date >.check-docker-sync
	@sleep 1
	@[[ "$$(docker run $(DOCKER_RUN_ARGS) $(DOCKER_TAG) cat .check-docker-sync | tr -d $$'\r')" == \
		  "$$(cat .check-docker-sync)" ]]

docker-aws-google-auth:
	@./scripts/run-aws-google-auth

show-startup-order:
	@find . -name 'S*'|grep etc/init.d|sed 's@\(.*\)etc/init.d/\(.*\)@\2 - \1etc/init.d/\2@'|sort

gen-variant-configs:
	@./scripts/gen-variant-configs

.PHONY: help
