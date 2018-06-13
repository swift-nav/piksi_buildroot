#!/bin/sh

has_tag() {
  git describe --exact-match --tags &>/dev/null
}

if [ -n "$FORCE_SDK_BUILD" ]; then
  echo ">>> We *SHOULD* create a buildroot SDK because FORCE_SDK_BUILD was set..."
  exit 1
else
  echo ">>> Found that FORCE_SDK_BUILD was NOT set..."
fi

if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
  if has_tag; then
    echo ">>> We *SHOULD* create a buildroot SDK because this isn't a pull request, and we found a tag..."
    exit 1
  else
    echo ">>> We *SHOULD NOT* create a buildroot SDK because this rev does not have a tag."
  fi
else
  echo ">>> We *SHOULD NOT* create a buildroot SDK because this is a PR build."
fi

exit 0
