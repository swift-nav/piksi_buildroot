# File: env-setup.mk

CCACHE_DIR := $(CURDIR)/buildroot/output/ccache

ifneq ($(CCACHE_READONLY),)
CCACHE_RO_VAR := CCACHE_READONLY=$(CCACHE_READONLY)
endif

BUILD_ENV_ARGS = \
  BR2_EXTERNAL=$(BR2_EXTERNAL) \
  BR2_HAS_PIKSI_INS_REF=$(BR2_HAS_PIKSI_INS_REF) \
  BR2_HAS_PIKSI_INS=$(BR2_HAS_PIKSI_INS) \
  BR2_BUILD_RELEASE_PROTECTED=$(BR2_BUILD_RELEASE_PROTECTED) \
  BR2_BUILD_RELEASE_OPEN=$(BR2_BUILD_RELEASE_OPEN) \
  BR2_BUILD_PIKSI_INS_REF=$(BR2_BUILD_PIKSI_INS_REF) \
  BR2_BUILD_PIKSI_INS=$(BR2_BUILD_PIKSI_INS) \
  BR2_BUILD_SAMPLE_DAEMON=$(BR2_BUILD_SAMPLE_DAEMON) \
  BR2_BUILD_STARLING_DAEMON=$(BR2_BUILD_STARLING_DAEMON) \
  BR2_CCACHE_DIR=$(CCACHE_DIR) \
  HW_CONFIG=$(HW_CONFIG) \
  $(CCACHE_RO_VAR)


ifeq ("$(OS)","Windows_NT")
USER := $(USERNAME)
UID  := 1000
GID  := 1000
else
UID := $(shell id -u)
GID := $(shell id -g)
endif

PIKSI_INS_REF_REPO := git@github.com:swift-nav/piksi_inertial_ipsec_crl.git

ifneq ($(BR2_BUILD_PIKSI_INS_REF),)
BR2_HAS_PIKSI_INS_REF := $(shell git ls-remote $(PIKSI_INS_REF_REPO) &>/dev/null && echo y)
endif

export BR2_HAS_PIKSI_INS_REF

PIKSI_INS_REPO :=  git@github.com:carnegieroboticsllc/piksi_ins.git

ifneq ($(BR2_BUILD_PIKSI_INS),)
BR2_HAS_PIKSI_INS := $(shell git ls-remote $(PIKSI_INS_REPO) &>/dev/null && echo y)
endif

export BR2_HAS_PIKSI_INS

LAST_GIT_TAG := $(shell git describe --abbrev=0 --tags)
