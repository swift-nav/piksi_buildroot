SHELL        := /bin/bash
BR2_EXTERNAL := $(shell pwd)

ifeq ($(HW_CONFIG),)
HW_CONFIG    := prod
endif

include docker/docker.mk

.PHONY: all firmware config image clean host-config host-image host-clean     \
        docker-setup docker-make-image docker-make-clean                      \
        docker-make-host-image docker-make-host-clean docker-run              \
        docker-config pkg-% docker-pkg-%                                      \

all: firmware image

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

docker-build-image:
	docker build --no-cache --force-rm \
		--build-arg VERSION_TAG=$(shell cat docker/version_tag) \
		--tag $(DOCKER_TAG) -f docker/Dockerfile .

docker-populate-volume:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		sudo chown -R $(USER) /piksi_buildroot
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		git submodule update --init --recursive

docker-setup: docker-build-image docker-populate-volume

docker-make-image: docker-config
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make image

docker-make-clean:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make clean

docker-make-clean-volume:
	docker volume rm $(DOCKER_BUILD_VOLUME)

docker-make-host-image:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make host-image

docker-make-host-clean:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make host-clean

docker-config:
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make -C buildroot O=output piksiv3_defconfig

docker-pkg-%: docker-config
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make -C buildroot $* O=output

docker-run:
	docker run $(DOCKER_RUN_ARGS) --name=$(DOCKER_TAG) \
		--tty --interactive $(DOCKER_TAG)

docker-cp:
	docker run $(DOCKER_RUN_ARGS) --name=piksi_buildroot_copy -d piksi_buildroot
	docker cp piksi_buildroot_copy:$(SRC) $(DST) || :
	docker stop piksi_buildroot_copy
