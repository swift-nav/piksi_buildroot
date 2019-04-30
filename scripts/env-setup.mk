# File: env-setup.mk

ifeq ($(VARIANT),)
$(info >>> Using default variant 'internal')
VARIANT := internal
else
$(info >>> Building for variant: $(VARIANT))
endif

VARIANT_OUTPUT := $(shell ./scripts/get-variant-prop $(VARIANT) output)
VARIANT_CONFIG := $(shell basename $(shell ./scripts/get-variant-prop $(VARIANT) config_output))

ifneq ($(VARIANT),host)

CCACHE_DIR = $(CURDIR)/buildroot/ccache

ifneq ($(CCACHE_READONLY),)
CCACHE_RO_VAR = CCACHE_READONLY=$(CCACHE_READONLY)
endif

endif

BUILD_ENV_ARGS = \
  BR2_EXTERNAL=$(BR2_EXTERNAL) \
  HW_CONFIG=$(HW_CONFIG) \
  $(CCACHE_RO_VAR)

UID := $(shell id -u)
GID := $(shell id -g)

LAST_GIT_TAG := $(shell git describe --abbrev=0 --tags)
