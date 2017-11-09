BR2_EXTERNAL:=$(shell pwd)

ifeq ($(HW_CONFIG),)
  HW_CONFIG=prod
endif

DOCKER_BUILD_VOLUME := piksi_buildroot-buildroot

DOCKER_RUN_ARGS :=                                                            \
  --rm                                                                        \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -e BR2_EXTERNAL=/piksi_buildroot                                            \
  -e GITHUB_TOKEN=$(GITHUB_TOKEN)                                             \
  -v $(HOME)/.ssh:/root/.ssh                                                  \
  ‭-v $(readlink -f $SSH_AUTH_SOCK):/ssh-agent -e SSH_AUTH_SOCK=/ssh-agent     \
  -v `pwd`:/piksi_buildroot                                                   \
  -v `pwd`/buildroot/output/images:/piksi_buildroot/buildroot/output/images   \
  -v $(DOCKER_BUILD_VOLUME):/piksi_buildroot/buildroot                        \

DOCKER_ARGS := --sig-proxy=false $(DOCKER_RUN_ARGS)

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
	docker build --no-cache --force-rm --tag piksi_buildroot .

docker-populate-volume:
	docker run $(DOCKER_ARGS) piksi_buildroot \
		git submodule update --init --recursive

docker-setup: docker-build-image docker-populate-volume

docker-make-image: docker-config
	docker run $(DOCKER_ARGS) piksi_buildroot \
		make image

docker-make-clean:
	docker run $(DOCKER_ARGS) piksi_buildroot \
		make clean

docker-make-clean-volume:
	docker volume rm $(DOCKER_BUILD_VOLUME)

docker-make-host-image:
	docker run $(DOCKER_ARGS) piksi_buildroot \
		make host-image

docker-make-host-clean:
	docker run $(DOCKER_ARGS) piksi_buildroot \
		make host-clean

docker-config:
	docker run $(DOCKER_ARGS) piksi_buildroot \
		make -C buildroot O=output piksiv3_defconfig

docker-pkg-%: docker-config
	docker run $(DOCKER_ARGS) piksi_buildroot \
		make -C buildroot $* O=output

docker-run:
	docker run $(DOCKER_RUN_ARGS) --name=piksi_buildroot -ti piksi_buildroot

docker-cp:
	docker run $(DOCKER_RUN_ARGS) --name=piksi_buildroot_copy -d piksi_buildroot
	docker cp piksi_buildroot_copy:$(SRC) $(DST) || :
	docker stop piksi_buildroot_copy
