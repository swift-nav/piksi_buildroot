#!/bin/bash

#switch to script dir
cd "${0%/*}"
#read version from mk file
grep LIBSWIFTNAV_VERSION libswiftnav.mk | cut -d '=' -f2 | xargs
