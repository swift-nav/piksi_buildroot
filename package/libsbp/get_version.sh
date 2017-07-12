#!/bin/bash

#switch to script dir
cd "${0%/*}"
#read version from mk file
grep LIBSBP_VERSION libsbp.mk | cut -d '=' -f2 | xargs
