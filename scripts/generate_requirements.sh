#!/bin/bash

# Copyright (C) 2016-2018 Swift Navigation Inc.
# Contact: Swift Navigation <dev@swiftnav.com>
#
# This source is subject to the license found in the file 'LICENSE' which must
# be be distributed together with this source. All other rights reserved.
#
# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
# EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
#
# Script for publishing yaml of external binary requirements to S3.

set -e

D=$( (cd "$(dirname "$0")" || exit 1 >/dev/null; pwd -P) )

GENERATE_REQUIREMENTS=y $D/../fetch_firmware.sh
$D/publish.sh $D/../requirements.yaml
