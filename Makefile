BR2_EXTERNAL:=$(shell pwd)

ifeq ($(HW_CONFIG),)
  HW_CONFIG=prod
endif

DOCKER_ARGS:=                                                                 \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -v `pwd`:/piksi_buildroot                                                   \
  -v `pwd`/buildroot/output/images:/piksi_buildroot/buildroot/output/images   \
  -v piksi_buildroot-buildroot:/piksi_buildroot/buildroot

.PHONY: all firmware config image host-config host-image                      \
        docker-setup docker-make-image docker-run travis

all: firmware image

firmware:
	./fetch_firmware.sh

config:
	BR2_EXTERNAL=$(BR2_EXTERNAL) \
		make -C buildroot O=output piksiv3_defconfig

image: config
	BR2_EXTERNAL=$(BR2_EXTERNAL) HW_CONFIG=$(HW_CONFIG) \
		make -C buildroot O=output

host-config:
	BR2_EXTERNAL=$(BR2_EXTERNAL) \
		make -C buildroot O=host_output host_defconfig

host-image: host-config
	BR2_EXTERNAL=$(BR2_EXTERNAL) \
		make -C buildroot O=host_output

docker-setup:
	docker build -t piksi_buildroot .
	docker run $(DOCKER_ARGS) --sig-proxy=false piksi_buildroot \
		git submodule update --init

docker-make-image:
	docker run $(DOCKER_ARGS) --sig-proxy=false piksi_buildroot \
		make image

docker-run:
	docker run $(DOCKER_ARGS) -ti piksi_buildroot

travis: firmware docker-setup
	HW_CONFIG=prod make docker-make-image 2>&1 \
		| tee -a build.out | grep --line-buffered '^>>>'
