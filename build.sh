#!/bin/bash
# Abort on Error
set -e

export BR2_EXTERNAL=..
export WORKDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export BUILD_OUTPUT=$WORKDIR/build.out

# Create empty output file, or clear it if it already exists
echo -n > $BUILD_OUTPUT

dump_output() {
   echo Tailing the last 500 lines of output:
   tail -500 $BUILD_OUTPUT  
}
error_handler() {
  echo ERROR: An error was encountered with the build.
  dump_output
  exit 1
}
# If an error occurs, run our error handler to output a tail of the build
trap 'error_handler' ERR

git clone --depth=1 git://git.buildroot.net/buildroot -b 2016.08
pushd buildroot
make piksiv3_defconfig >> $BUILD_OUTPUT 2>&1
make >> $BUILD_OUTPUT 2>&1
popd

# The build finished without returning an error so dump a tail of the output
dump_output

