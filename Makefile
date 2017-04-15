BR2_EXTERNAL:=$(shell pwd)

ifeq ($(HW_CONFIG),)
  HW_CONFIG=prod
endif

ifeq ($(BUILDROOT_CONFIG),)
  BUILDROOT_CONFIG=piksiv3_defconfig
endif

DOCKER_ARGS:=                                                                 \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -v `pwd`:/piksi_buildroot                                                   \
  -v `pwd`/buildroot/output/images:/piksi_buildroot/buildroot/output/images   \
  -v piksi_buildroot-buildroot:/piksi_buildroot/buildroot

.PHONY: all firmware config image docker-setup docker-make-image docker-run test cmake-setup travis

all: firmware image

firmware:
	./fetch_firmware.sh

config:
	BR2_EXTERNAL=$(BR2_EXTERNAL) make -C buildroot $(BUILDROOT_CONFIG)

image: config
	BR2_EXTERNAL=$(BR2_EXTERNAL) HW_CONFIG=$(HW_CONFIG) make -C buildroot

docker-setup:
	docker build -t piksi_buildroot .
	docker run $(DOCKER_ARGS) piksi_buildroot git submodule update --init

docker-make-image:
	docker run $(DOCKER_ARGS) piksi_buildroot make image

docker-run:
	docker run $(DOCKER_ARGS) -ti piksi_buildroot

cmake-setup:
	mkdir -p build && cd build && cmake ..

test: cmake-setup
	make -C build
	make -C build test

travis: firmware docker-setup
	HW_CONFIG=prod make docker-make-image 2>&1 | tee -a build.out | grep --line-buffered '^make'
	HW_CONFIG=microzed make docker-make-image 2>&1 | tee -a build.out | grep --line-buffered '^make'
