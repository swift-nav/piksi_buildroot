BR2_EXTERNAL:=$(shell pwd)

ifeq ($(HW_CONFIG),)
  HW_CONFIG=prod
endif

ifeq ($(BUILDROOT_CONFIG),)
  BUILDROOT_CONFIG=piksiv3_defconfig
endif

.PHONY: all firmware config image

all: firmware image

firmware:
	./fetch_firmware.sh

config:
	BR2_EXTERNAL=$(BR2_EXTERNAL) make -C buildroot $(BUILDROOT_CONFIG)

image: config
	BR2_EXTERNAL=$(BR2_EXTERNAL) HW_CONFIG=$(HW_CONFIG) make -C buildroot

