SHELL        := /bin/bash
BR2_EXTERNAL := $(CURDIR)

ifeq ($(HW_CONFIG),)
HW_CONFIG    := prod
endif

.PHONY: all firmware config image clean host-config host-image host-clean     \
        docker-setup docker-make-image docker-make-clean                      \
        docker-make-host-image docker-make-host-clean docker-run              \
        docker-config pkg-% docker-pkg-%                                      \
        docker-rebuild-changed rebuild-changed _rebuild_changed               \

all: firmware image

include scripts/docker.mk

firmware:
	./fetch_firmware.sh

config:
	BR2_EXTERNAL=$(BR2_EXTERNAL) \
		make -C buildroot O=output piksiv3_defconfig

image: config
	BR2_EXTERNAL=$(BR2_EXTERNAL) HW_CONFIG=$(HW_CONFIG) \
		make -C buildroot O=output

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
	BR2_EXTERNAL=$(BR2_EXTERNAL) HW_CONFIG=$(HW_CONFIG) \
		make -C buildroot $* O=output

host-pkg-%: host-config
	BR2_EXTERNAL=$(BR2_EXTERNAL) \
		make -C buildroot $* O=host_output

host-config:
	BR2_EXTERNAL=$(BR2_EXTERNAL) \
		make -C buildroot O=host_output host_defconfig

host-image: host-config
	BR2_EXTERNAL=$(BR2_EXTERNAL) \
		make -C buildroot O=host_output

host-clean:
	rm -rf buildroot/host_output

rebuild-changed: export BUILD_TEMP=/tmp SINCE=$(SINCE)
rebuild-changed: _rebuild_changed

_rebuild_changed:
	BR2_EXTERNAL=$(BR2_EXTERNAL) HW_CONFIG=$(HW_CONFIG) \
		make -C buildroot \
			$(shell BUILD_TEMP=$(BUILD_TEMP) SINCE=$(SINCE) scripts/changed_project_targets.py) \
			O=output

_print_db:
	BR2_EXTERNAL=$(BR2_EXTERNAL) HW_CONFIG=$(HW_CONFIG) \
		make -C buildroot all O=output -np

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

docker-make-image: docker-config
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make image

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

docker-config:
	docker run $(DOCKER_RUN_ARGS) $(DOCKER_TAG) \
		make -C buildroot O=output piksiv3_defconfig

docker-pkg-%: docker-config
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make -C buildroot $* O=output

docker-run:
	docker run $(DOCKER_RUN_ARGS) --name=$(DOCKER_TAG) \
		--tty --interactive $(DOCKER_TAG) || :

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

docker-export-build-output:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		bash -c 'tar -cjf /tmp/build-output.tbz2 buildroot/output/build && cp /tmp/build-output.tbz2 .'
