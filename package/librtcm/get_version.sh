#!/bin/bash

#switch to script dir
cd "${0%/*}"
#read version from mk file
grep LIBRTCM_VERSION librtcm.mk | cut -d '=' -f2 | xargs
