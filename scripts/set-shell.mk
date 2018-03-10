ifneq (,$(findstring NixOS,$(shell uname -a)))
	FOUND_NIX := y
else
	FOUND_NIX := n
endif

ifneq (1,$(IN_NIX_SHELL))
	FOUND_NO_NIX_SHELL := n
else
	FOUND_NO_NIX_SHELL := y
endif

ifneq (1,$(IN_PIKSI_SHELL))
	FOUND_NO_PIKSI_SHELL := n
else
	FOUND_NO_PIKSI_SHELL := y
endif

ifeq (ynn,$(FOUND_NIX)$(FOUND_NO_NIX_SHELL)$(FOUND_NO_PIKSI_SHELL))
SHELL        := $(CURDIR)/scripts/nixwrap.bash
else
SHELL        := $(shell which bash)
endif
