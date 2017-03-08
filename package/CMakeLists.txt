cmake_minimum_required(VERSION 2.8.7)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("cmake/zmq.cmake")
include("cmake/sbp.cmake")

add_subdirectory(libsbp_zmq/src)

# Testing
set (PACKAGE_BUILD_TESTS ON CACHE BOOL "Build the package tests")

if (PACKAGE_BUILD_TESTS)
  include("cmake/gtest.cmake")
  # Enable testing only works in root scope
  enable_testing ()
endif (PACKAGE_BUILD_TESTS)
