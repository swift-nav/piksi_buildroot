# Top-level Makefile for piksi_buildroot

BR2_EXTERNAL := $(CURDIR)
BR2_DL_DIR   := $(CURDIR)/output/dl

export BR2_EXTERNAL
export BR2_DL_DIR

ifeq ($(HW_CONFIG),)
  HW_CONFIG=prod
endif

export HW_CONFIG

DOCKER_BUILD_VOLUME := piksi_buildroot-buildroot$(DOCKER_SUFFIX)
DOCKER_TAG := piksi_buildroot$(DOCKER_SUFFIX)

DOCKER_RUN_ARGS :=                                                            \
  --rm                                                                        \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -e BR2_EXTERNAL=/piksi_buildroot                                            \
  -e BR2_DL_DIR=/piksi_buildroot/output/dl                                    \
  -v `pwd`:/piksi_buildroot                                                   \
  -v `pwd`/output/target/images:/piksi_buildroot/output/target/images   \
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
	make -C buildroot O=$(CURDIR)/output/target piksiv3_defconfig

image: config
	make -C buildroot O=$(CURDIR)/output/target

clean:
	find output/target -mindepth 1 -maxdepth 1 \
		! -path output/target/images -print -exec rm -rf {} \;
	rm -rf output/target/images/*

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
	make -C buildroot $* O=$(CURDIR)/output/target

host-pkg-%: host-config
	make -C buildroot $* O=$(CURDIR)/output/host

host-config:
	make -C buildroot O=$(CURDIR)/output/host host_defconfig

host-image: host-config
	make -C buildroot O=$(CURDIR)/output/host

host-clean:
	rm -rf output/host

docker-build-image:
	docker build --no-cache --force-rm --tag $(DOCKER_TAG) -f docker/Dockerfile .

docker-populate-volume:
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
		make -C buildroot O=/piksi_buildroot/output piksiv3_defconfig

docker-pkg-%: docker-config
	docker run $(DOCKER_ARGS) $(DOCKER_TAG) \
		make -C buildroot $* O=/piksi_buidlroot/output

docker-run:
	docker run $(DOCKER_RUN_ARGS) --name=$(DOCKER_TAG) -ti $(DOCKER_TAG)
