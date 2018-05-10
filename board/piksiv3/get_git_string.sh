#!/bin/sh
set -e
git -C $BR2_EXTERNAL_piksi_buildroot_PATH describe --tags        \
                 --dirty --always --match 'v[0-9]*\.[0-9]*\.[0-9]*'
