DOCKER_BUILD_VOLUME = piksi_buildroot-buildroot$(DOCKER_SUFFIX)
DOCKER_TAG = piksi_buildroot$(DOCKER_SUFFIX)

PIKSI_INS_REF_REPO := git@github.com:swift-nav/piksi_inertial_ipsec_crl.git
BR2_HAS_PIKSI_INS_REF := $(shell git ls-remote $(PIKSI_INS_REF_REPO) &>/dev/null && echo y)

export BR2_HAS_PIKSI_INS_REF

DOCKER_RUN_ARGS :=                                                            \
  --rm                                                                        \
  -e USER=$(USER)                                                             \
  -e UID=$(shell id -u)                                                       \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -e BR2_EXTERNAL=/piksi_buildroot                                            \
  -e BR2_HAS_PIKSI_INS_REF=$(BR2_HAS_PIKSI_INS_REF)                           \
  -e GITHUB_TOKEN=$(GITHUB_TOKEN)                                             \
  --hostname piksi-builder$(DOCKER_SUFFIX)                                    \
  -v $(HOME)/.ssh:/host-ssh:ro                                                \
  -v `pwd`:/piksi_buildroot                                                   \
  -v `pwd`/buildroot/output/images:/piksi_buildroot/buildroot/output/images   \
  -v $(DOCKER_BUILD_VOLUME):/piksi_buildroot/buildroot                        \

ifneq ($(SSH_AUTH_SOCK),)
DOCKER_RUN_ARGS := $(DOCKER_RUN_ARGS) -v $(shell python -c "print(__import__('os').path.realpath('$(SSH_AUTH_SOCK)'))"):/ssh-agent -e SSH_AUTH_SOCK=/ssh-agent
endif

DOCKER_ARGS := --sig-proxy=false $(DOCKER_RUN_ARGS)
