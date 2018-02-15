# File: docker.mk
#
# Setup variables for invoking docker with sane settings for a development
#   environement.

ifneq ("$(DOCKER_SUFFIX)","")
_DOCKER_SUFFIX := -$(DOCKER_SUFFIX)
endif

ifeq ("$(OS)","Windows_NT")
USER := $(USERNAME)
UID  := 1000
GID  := 1000
else
UID := $(shell id -u)
GID := $(shell id -g)
endif

DOCKER_BUILD_VOLUME = piksi_buildroot-$(USER)$(_DOCKER_SUFFIX)
DOCKER_TAG = piksi_buildroot-$(USER)$(_DOCKER_SUFFIX)

PIKSI_INS_REF_REPO := git@github.com:swift-nav/piksi_inertial_ipsec_crl.git
#BR2_HAS_PIKSI_INS_REF := $(shell git ls-remote $(PIKSI_INS_REF_REPO) &>/dev/null && echo y)

export BR2_HAS_PIKSI_INS_REF

ifneq ($(AWS_ACCESS_KEY_ID),)
AWS_VARIABLES := -e AWS_ACCESS_KEY_ID=$(AWS_ACCESS_KEY_ID)
endif

ifneq ($(AWS_SECRET_ACCESS_KEY),)
AWS_VARIABLES := $(AWS_VARIABLES) -e AWS_SECRET_ACCESS_KEY=$(AWS_SECRET_ACCESS_KEY)
endif

ifeq ($(PIKSI_NON_INTERACTIVE_BUILD),)
INTERACTIVE_ARGS := $(shell tty &>/dev/null && echo "--tty --interactive")
endif

DOCKER_SETUP_ARGS :=                                                          \
  $(INTERACTIVE_ARGS)                                                         \
  --rm                                                                        \
  -e USER=$(USER)                                                             \
  -e GID=$(GID)                                                               \
  -e HW_CONFIG=$(HW_CONFIG)                                                   \
  -e BR2_EXTERNAL=/piksi_buildroot                                            \
  -e BR2_HAS_PIKSI_INS_REF=$(BR2_HAS_PIKSI_INS_REF)                           \
  -e BR2_BUILD_SAMPLE_DAEMON=$(BR2_BUILD_SAMPLE_DAEMON)                       \
  -e GITHUB_TOKEN=$(GITHUB_TOKEN)                                             \
  $(AWS_VARIABLES)                                                            \
  --hostname piksi-builder$(_DOCKER_SUFFIX)                                   \
  --user $(USER)                                                              \
  -v $(HOME):/host/home:ro                                                    \
  -v /tmp:/host/tmp:rw                                                        \
  -v $(CURDIR):/piksi_buildroot                                               \
  -v $(DOCKER_BUILD_VOLUME):/piksi_buildroot/buildroot

DOCKER_RUN_ARGS := \
  $(DOCKER_SETUP_ARGS) \
  -v $(CURDIR)/buildroot/output/images:/piksi_buildroot/buildroot/output/images

ifneq ($(SSH_AUTH_SOCK),)
DOCKER_RUN_ARGS := $(DOCKER_RUN_ARGS) -v $(shell python -c "print(__import__('os').path.realpath('$(SSH_AUTH_SOCK)'))"):/ssh-agent -e SSH_AUTH_SOCK=/ssh-agent
endif

DOCKER_ARGS := --sig-proxy=false $(DOCKER_RUN_ARGS)

docker-wipe:
	@echo "WARNING: This will wipe all Piksi related Docker images, containers, and volumes!"
	@read -p "Continue? (y/[n]) " x && { [[ "$$x" == "y" ]] || exit 0; } && \
		echo -n "Wiping all Piksi related docker materials" && \
		{ sleep 0.25; echo -n .; sleep 0.25; echo -n .; sleep 0.25; echo -n .; sleep 0.25; echo; } && \
		{ \
			echo "... stopping Piksi containers"; \
			running_images=`docker ps --format '{{.Names}},{{.ID}}' | grep '^piksi_.*,.*$$' | cut -f2 -d,`; \
			[[ -z "$$running_images" ]] || docker stop -t 1 $$running_images; \
			echo "... removing Piksi containers"; \
			stopped_images=`docker ps -a --format '{{.Names }},{{.ID}}' | grep '^piksi_.*,.*$$' | cut -f2 -d,`; \
			[[ -z "$$stopped_images" ]] || docker rm -f $$stopped_images; \
			echo "... removing Piksi images"; \
			images=`docker images --format '{{.Repository}},{{.ID}}' | grep '^piksi_.*,.*$$' | cut -f2 -d,`; \
			[[ -z "$$images" ]] || docker rmi -f $$images; \
			echo "... removing Piksi volumes"; \
			volumes=`docker volume ls --format '{{.Name}}' | grep '^piksi_.*$$'`; \
			[[ -z "$$volumes" ]] || docker volume rm $$volumes; \
		}

.PHONY: docker-wipe
