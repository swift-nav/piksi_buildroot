BR2_EXTERNAL:=$(shell pwd)

ifeq ($(HW_CONFIG),)
  HW_CONFIG=prod
endif

ifeq ($(BUILDROOT_CONFIG),)
  BUILDROOT_CONFIG=piksiv3_defconfig
endif

.PHONY: all firmware config image

all: firmware config image

firmware:
	./fetch_firmware.sh

config:
	make -C buildroot $(BUILDROOT_CONFIG)

image:
	make -C buildroot

