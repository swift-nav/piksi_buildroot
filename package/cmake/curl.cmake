cmake_minimum_required(VERSION 2.8)

# This brings in the external project support in cmake
include(ExternalProject)

# build directory
set(CURL_PREFIX ${CMAKE_BINARY_DIR}/external/CURL-prefix)
# install directory
set(CURL_INSTALL ${CMAKE_BINARY_DIR}/external/CURL-install)
# This adds zmq as an external project with the specified parameters.
ExternalProject_Add(libcurl
  PREFIX ${CURL_PREFIX}
  # We use SOURCE_DIR because we use version control to track the
  # version of this library instead of using the build tool
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/libcurl
  INSTALL_DIR ${CURL_INSTALL}
  #INSTALL_COMMAND cmake -E echo "Not installing libcurl globally."
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CURL_INSTALL}
  # This simply passes down cmake arguments, which allows us to define
  # zmq-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})
set(CURL_INCLUDE_DIRS ${CURL_INSTALL}/include)
set(CURL_LIBRARY_DIRS ${CURL_INSTALL}/lib)
