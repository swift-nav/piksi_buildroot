cmake_minimum_required(VERSION 2.8)

# This brings in the external project support in cmake
include(ExternalProject)

# build directory
set(SBP_PREFIX ${CMAKE_BINARY_DIR}/external/SBP-prefix)
# install directory
set(SBP_INSTALL ${CMAKE_BINARY_DIR}/external/SBP-install)
# This adds SBP as an external project with the specified parameters.
ExternalProject_Add(libsbp
  PREFIX ${SBP_PREFIX}
  # We use SOURCE_DIR because we use version control to track the
  # version of this library instead of using the build tool
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/libsbp/c
  INSTALL_DIR ${SBP_INSTALL}
  #INSTALL_COMMAND cmake -E echo "Not installing libsbp globally."
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${SBP_INSTALL}
  # This simply passes down cmake arguments, which allows us to define
  # SBP-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})
set(LIBSBP_INCLUDE_DIRS ${SBP_INSTALL}/include)
set(LIBSBP_LIBRARY_DIRS ${SBP_INSTALL}/lib)
