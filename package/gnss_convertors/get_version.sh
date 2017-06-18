#!/bin/bash

#switch to script dir
cd "${0%/*}"
#read version from mk file
grep GNSS_CONVERTORS_VERSION gnss_convertors.mk | cut -d '=' -f2 | xargs
