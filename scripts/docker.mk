# File: docker.mk
#
# Setup variables for invoking docker with sane settings for a development
#   environement.

SANITIZED_USER = $(shell echo $(USER) | tr -d .)
LOWER_USER = $(shell echo $(SANITIZED_USER) | tr A-Z a-z)

ifneq ("$(DOCKER_SUFFIX)","")
_DOCKER_SUFFIX := -$(LOWER_USER)-$(DOCKER_SUFFIX)
else
_DOCKER_SUFFIX := -$(shell pwd | $(CURDIR)/scripts/gen-docker-suffix)
endif

ifneq ($(AWS_ACCESS_KEY_ID),)
AWS_VARIABLES := -e AWS_ACCESS_KEY_ID=$(AWS_ACCESS_KEY_ID)
endif

ifneq ($(AWS_SECRET_ACCESS_KEY),)
AWS_VARIABLES := $(AWS_VARIABLES) -e AWS_SECRET_ACCESS_KEY=$(AWS_SECRET_ACCESS_KEY)
endif

ifneq ($(CCACHE_READONLY),)
DOCKER_CCACHE_RO_VAR := -e CCACHE_READONLY=$(CCACHE_READONLY)
endif

ifeq ($(PIKSI_NON_INTERACTIVE_BUILD),)
INTERACTIVE_ARGS := $(shell tty &>/dev/null && echo "--tty --interactive")
endif

DOCKER_BUILD_VOLUME = piksi-buildroot$(_DOCKER_SUFFIX)
DOCKER_TAG = piksi-buildroot$(_DOCKER_SUFFIX)

DOCKER_ENV_ARGS =                                                             \
  -e USER=$(SANITIZED_USER)                                                   \
  -e GID=$(GID)                                                               \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -e VARIANT=$(VARIANT)                                                       \
  -e BR2_EXTERNAL=/piksi_buildroot                                            \
  -e GITHUB_TOKEN=$(GITHUB_TOKEN)                                             \
  -e DISABLE_NIXOS_SUPPORT=$(DISABLE_NIXOS_SUPPORT)                           \
  $(AWS_VARIABLES)                                                            \
  $(DOCKER_CCACHE_RO_VAR)                                                     \
  --user $(SANITIZED_USER)                                                    \

ifeq (,$(wildcard .docker-sync.yml))

BUILD_VOLUME_ARGS = \
  -v $(CURDIR):/piksi_buildroot \
  -v $(DOCKER_BUILD_VOLUME):/piksi_buildroot/buildroot

RUN_VOLUME_ARGS = \
  -v $(CURDIR)/buildroot/output_$(VARIANT)/images:/piksi_buildroot/buildroot/output_$(VARIANT)/images

else

BUILD_VOLUME_ARGS := \
  -v $(DOCKER_BUILD_VOLUME)-sync:/piksi_buildroot

RUN_VOLUME_ARGS :=

endif

DOCKER_SETUP_ARGS :=                                                          \
  $(INTERACTIVE_ARGS)                                                         \
  $(DOCKER_ENV_ARGS)                                                          \
  --rm                                                                        \
  --hostname piksi-buildroot$(_DOCKER_SUFFIX)                                 \
  -v $(HOME):/host/home:ro                                                    \
  -v /tmp:/host/tmp:rw                                                        \
  $(BUILD_VOLUME_ARGS)

DOCKER_RUN_ARGS := \
  $(DOCKER_SETUP_ARGS) \
  $(RUN_VOLUME_ARGS) \
  --security-opt seccomp:unconfined --cap-add=SYS_PTRACE

realpath = $(shell python -c "print(__import__('os').path.realpath('$(1)'))")

ifneq ($(SSH_AUTH_SOCK),)
DOCKER_RUN_ARGS := $(DOCKER_RUN_ARGS) -v $(call realpath,$(SSH_AUTH_SOCK)):/ssh-agent -e SSH_AUTH_SOCK=/ssh-agent
endif

DOCKER_ARGS := --sig-proxy=false $(DOCKER_RUN_ARGS)

docker-wipe:
	@./scripts/run-docker-wipe

.PHONY: docker-wipe
