#!/bin/bash

#switch to script dir
cd "${0%/*}"
#read version from mk file
grep GNSS_CONVERTERS_VERSION gnss_converters.mk | cut -d '=' -f2 | xargs
