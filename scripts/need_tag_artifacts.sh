#!/usr/bin/env bash

[[ -z "$DEBUG" ]] || set -x

has_tag() {
  git describe --exact-match --tags &>/dev/null
}

should_build_toolchain=

if [[ -n "$FORCE_TOOLCHAIN_BUILD" ]] || [[ -n "$FORCE_CCACHE_BUILD" ]]; then
  echo ">>> We *SHOULD* create a buildroot toolchain because FORCE_TOOLCHAIN_BUILD (or FORCE_CCACHE_BUILD) was set..."
  should_build_toolchain=y
else
  echo ">>> Found that FORCE_TOOLCHAIN_BUILD (and FORCE_CCACHE_BUILD) was NOT set..."
fi

if [[ "$TRAVIS_PULL_REQUEST" == "false" ]]; then
  if has_tag; then
    echo ">>> We *SHOULD* create an SDK/ccache because this isn't a pull request, and we found a tag..."
    should_build_toolchain=y
  else
    echo ">>> We *SHOULD NOT* create a SDK/ccache because this rev does not have a tag."
  fi
else
  echo ">>> We *SHOULD NOT* create a SDK/ccache because this is a PR build."
fi

[[ -n "$should_build_toolchain" ]]
