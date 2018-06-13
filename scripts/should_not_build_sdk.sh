#!/usr/bin/env bash

[[ -z "$DEBUG" ]] || set -x

has_tag() {
  git describe --exact-match --tags &>/dev/null
}

should_build_sdk=

if [[ -n "$FORCE_SDK_BUILD" ]]; then
  echo ">>> We *SHOULD* create a buildroot SDK because FORCE_SDK_BUILD was set..."
  should_build_sdk=y
else
  echo ">>> Found that FORCE_SDK_BUILD was NOT set..."
fi

if [[ "$TRAVIS_PULL_REQUEST" == "false" ]]; then
  if has_tag; then
    echo ">>> We *SHOULD* create a buildroot SDK because this isn't a pull request, and we found a tag..."
    should_build_sdk=y
  else
    echo ">>> We *SHOULD NOT* create a buildroot SDK because this rev does not have a tag."
  fi
else
  echo ">>> We *SHOULD NOT* create a buildroot SDK because this is a PR build."
fi

# Invert to answer the "not" question
[[ -n "$should_build_sdk" ]] && exit 1 || exit 0
