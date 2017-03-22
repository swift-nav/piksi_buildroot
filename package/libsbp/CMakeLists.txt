cmake_minimum_required(VERSION 2.8)

# This brings in the external project support in cmake
include(ExternalProject)

#parse git version from mk file
execute_process(COMMAND "${CMAKE_SOURCE_DIR}/package/libsbp/get_version.sh" OUTPUT_VARIABLE MK_LIBSBP_VERSION)

# build directory
set(SBP_PREFIX ${CMAKE_BINARY_DIR}/external/SBP-prefix)
# install directory
set(SBP_INSTALL ${CMAKE_BINARY_DIR}/external/SBP-install)
# This adds SBP as an external project with the specified parameters.
ExternalProject_Add(libsbp
  PREFIX ${SBP_PREFIX}
  # Pulls version the matches buildroot
  GIT_REPOSITORY https://github.com/swift-nav/libsbp
  GIT_TAG ${MK_LIBSBP_VERSION}
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -G${CMAKE_GENERATOR} "${SBP_PREFIX}/src/libsbp/c/" -DCMAKE_INSTALL_PREFIX=${SBP_INSTALL}
  INSTALL_DIR ${SBP_INSTALL}
  # This simply passes down cmake arguments, which allows us to define
  # SBP-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})
set(LIBSBP_INCLUDE_DIRS ${SBP_INSTALL}/include)
set(LIBSBP_LIBRARY_DIRS ${SBP_INSTALL}/lib)
