include scripts/set-shell.mk
include scripts/env-setup.mk

BR2_EXTERNAL := $(CURDIR)

ifeq ($(HW_CONFIG),)
HW_CONFIG    := prod
endif

.PHONY: all firmware config image clean host-config host-image host-clean     \
        docker-setup docker-make-image docker-make-clean                      \
        docker-make-host-image docker-make-host-clean docker-run              \
        docker-host-config docker-config pkg-% docker-pkg-%                   \
        docker-rebuild-changed rebuild-changed _rebuild_changed               \
				docker-host-pkg-%                                                     \

all: firmware image

include scripts/docker.mk

firmware:
	./fetch_firmware.sh

config:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=output piksiv3_defconfig

dev-tools-clean: pkg-piksi_dev_tools-dirclean

dev-tools-build: pkg-piksi_dev_tools

rel-lockdown-clean: pkg-release_lockdown-dirclean

POST_IMAGE_ENV = HW_CONFIG=prod \
                 BINARIES_DIR=buildroot/output/images \
                 TARGET_DIR=buildroot/output/target \
                 BUILD_DIR=buildroot/output/build \
                 BR2_EXTERNAL_piksi_buildroot_PATH=$(PWD)

define _release_build
	$(BUILD_ENV_ARGS) \
		$(MAKE) flush-rootfs rel-lockdown-clean
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=output V=$(V) uboot_custom
	@$(POST_IMAGE_ENV) BR2_JUST_GEN_FAILSAFE=y ./board/piksiv3/post_image.sh
	$(1)
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=output V=$(V)
endef

define _release_ins_build
	[ -z "$(BR2_BUILD_PIKSI_INS)" ] || \
		$(BUILD_ENV_ARGS) \
			$(MAKE) pkg-piksi_ins-rebuild
endef

image-release-open: export BR2_BUILD_RELEASE_OPEN=y
image-release-open: config
	$(call _release_build,:)

image-release-protected: export BR2_BUILD_RELEASE_PROTECTED=y
image-release-protected: config
	$(call _release_build,$(_release_ins_build))

image-release-ins: export BR2_BUILD_PIKSI_INS=y
image-release-ins:
	$(BUILD_ENV_ARGS) \
		$(MAKE) image-release-protected

image: export BR2_BUILD_PIKSI_INS=y
image: config
	$(BUILD_ENV_ARGS) BR2_BUILD_RELEASE_OPEN=y \
		$(MAKE) rel-lockdown-clean
	$(BUILD_ENV_ARGS) \
		$(MAKE) dev-tools-clean dev-tools-build
	$(_release_ins_build)
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=output V=$(V)

clean:
	find buildroot/output -mindepth 1 -maxdepth 1 \
		! -path buildroot/output/images -print -exec rm -rf {} \;
	rm -rf buildroot/output/images/*

flush-rootfs:
	find buildroot/output -name .stamp_target_installed -delete
	rm -rf buildroot/output/target/*

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
		$(MAKE) -C buildroot $* O=output

host-pkg-%: host-config
	BR2_EXTERNAL=$(BR2_EXTERNAL) BR2_DISABLE_LTO=y \
		$(MAKE) -C buildroot $* O=host_output

host-config:
	BR2_EXTERNAL=$(BR2_EXTERNAL) BR2_DISABLE_LTO=y \
		$(MAKE) -C buildroot O=host_output host_defconfig

host-image: host-config
	BR2_EXTERNAL=$(BR2_EXTERNAL) BR2_DISABLE_LTO=y \
		$(MAKE) -C buildroot O=host_output

host-clean:
	rm -rf buildroot/host_output

rebuild-changed: export BUILD_TEMP=/tmp SINCE=$(SINCE)
rebuild-changed: _rebuild_changed

REBUILD_CHANGED_IGNORE := release_lockdown|piksi_ins|sample_daemon|llvm_o|llvm_v

_rebuild_changed:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot \
			$(shell BUILD_TEMP=$(BUILD_TEMP) SINCE=$(SINCE) \
				scripts/changed_project_targets.py | grep -v -E '($(REBUILD_CHANGED_IGNORE))') \
			O=output

_print_db:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot all O=output -np

docker-build-image:
	docker build --no-cache --force-rm \
		--build-arg VERSION_TAG=$(shell cat scripts/docker_version_tag) \
		--build-arg USER=$(USER) \
		--build-arg UID=$(UID) \
		--build-arg GID=$(GID) \
		--tag $(DOCKER_TAG) -f scripts/Dockerfile .

docker-populate-volume:
	docker run $(DOCKER_SETUP_ARGS) $(DOCKER_TAG) \
		git submodule update --init --recursive

docker-setup: docker-build-image docker-populate-volume

docker-rebuild-changed:
	docker run $(DOCKER_ARGS) -e BUILD_TEMP=/host/tmp -e SINCE=$(SINCE) \
		$(DOCKER_TAG) \
		make _rebuild_changed

docker-make-image:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make image

docker-make-image-release-open:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make image-release-open

docker-make-image-release-protected:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make image-release-protected

docker-make-image-release-ins:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make image-release-ins

docker-make-clean:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make clean

docker-make-clean-volume:
	docker volume rm $(DOCKER_BUILD_VOLUME)

docker-make-clean-all: docker-make-clean docker-make-clean-volume

docker-make-host-image:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make host-image

docker-make-host-clean:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make host-clean

docker-host-config:
	docker run -e BR2_DISABLE_LTO=y $(DOCKER_RUN_ARGS) $(DOCKER_TAG) \
		make -C buildroot O=host_output host_defconfig

docker-config:
	docker run $(DOCKER_RUN_ARGS) $(DOCKER_TAG) \
		make -C buildroot O=output piksiv3_defconfig

docker-host-pkg-%: docker-host-config
	docker run -e BR2_DISABLE_LTO=y $(DOCKER_ARGS) $(DOCKER_TAG) \
		make -C buildroot $* O=host_output

docker-pkg-%: docker-config
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make -C buildroot $* O=output

docker-run:
	docker run $(DOCKER_RUN_ARGS) --name=$(DOCKER_TAG) \
		--tty --interactive $(DOCKER_TAG) || :

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

help:
	@[[ -x $(shell which less) ]] && \
		less $(CURDIR)/scripts/make_help.txt || \
		cat $(CURDIR)/scripts/make_help.txt

clang-complete:
	@./scripts/gen-clang-complete

docker-make-sdk:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make sdk

sdk:
	$(BUILD_ENV_ARGS) \
		$(MAKE) -C buildroot O=output V=$(V) sdk
	@echo '>>>' Uninstalling piksi toolchain wrappers...
	$(MAKE) -C buildroot force-uninstall-toolchain-wrappers
	@echo '>>>' Creating SDK archive...
	tar -cJf piksi_sdk.txz -C buildroot/output/host .

.PHONY: help
