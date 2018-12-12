#!/bin/bash

#switch to script dir
cd "${0%/*}"
#read version from mk file
grep LIBSETTINGS_VERSION libsettings.mk | cut -d '=' -f2 | xargs
