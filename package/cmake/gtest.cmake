cmake_minimum_required(VERSION 2.8)

# This brings in the external project support in cmake
include(ExternalProject)

# build directory
set(GTEST_PREFIX ${CMAKE_BINARY_DIR}/external/GTEST-prefix)
# install directory
set(GTEST_INSTALL ${CMAKE_BINARY_DIR}/external/GTEST-install)
# This adds zmq as an external project with the specified parameters.
ExternalProject_Add(googletest-distribution
  PREFIX ${GTEST_PREFIX}
  # We use SOURCE_DIR because we use version control to track the
  # version of this library instead of using the build tool
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/googletest
  INSTALL_DIR ${GTEST_INSTALL}
  #INSTALL_COMMAND cmake -E echo "Not installing ZeroMQ globally."
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL}
  # This simply passes down cmake arguments, which allows us to define
  # zmq-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})
set(GTEST_INCLUDE_DIRS ${GTEST_INSTALL}/include)
set(GTEST_LIBRARY_DIRS ${GTEST_INSTALL}/lib)
