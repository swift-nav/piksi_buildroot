BR2_EXTERNAL:=$(shell pwd)

ifeq ($(HW_CONFIG),)
  HW_CONFIG=prod
endif

DOCKER_ARGS:=                                                                 \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -v `pwd`:/piksi_buildroot                                                   \
  -v `pwd`/buildroot/output/images:/piksi_buildroot/buildroot/output/images   \
  -v piksi_buildroot-buildroot:/piksi_buildroot/buildroot

.PHONY: all firmware config image clean host-config host-image host-clean     \
        docker-setup docker-make-image docker-make-clean                      \
        docker-make-host-image docker-make-host-clean docker-run

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
		\( ! -path buildroot/output/images -and -type f \) -delete
	find buildroot/output -mindepth 1 -maxdepth 1 \
		\( ! -path buildroot/output/images -and -type d \) -delete
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

docker-setup:
	docker build -t piksi_buildroot .
	docker run $(DOCKER_ARGS) --sig-proxy=false piksi_buildroot \
		git submodule update --init

docker-make-image:
	docker run $(DOCKER_ARGS) --sig-proxy=false piksi_buildroot \
		make image

docker-make-clean:
	docker run $(DOCKER_ARGS) --sig-proxy=false piksi_buildroot \
		make clean

docker-make-host-image:
	docker run $(DOCKER_ARGS) --sig-proxy=false piksi_buildroot \
		make host-image

docker-make-host-clean:
	docker run $(DOCKER_ARGS) --sig-proxy=false piksi_buildroot \
		make host-clean

BR_DOCKER_NAME ?= piksi_buildroot

docker-run:
	docker run $(DOCKER_ARGS) --name $(BR_DOCKER_NAME) -it --rm piksi_buildroot
