#!/bin/bash

#switch to script dir
cd "${0%/*}"
#read version from mk file
grep PIKSI_APPS_VERSION piksi_apps.mk | cut -d '=' -f2 | xargs
