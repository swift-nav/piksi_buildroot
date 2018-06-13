#!/bin/sh

[ -z "$FORCE_SDK_BUILD" ] || exit 1

[ "$TRAVIS_PULL_REQUEST" != "false" ] || [ -z "$TRAVIS_TAG" ]
