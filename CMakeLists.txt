cmake_minimum_required(VERSION 2.8.7)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("cmake/zmq.cmake")
include("package/libsbp/CMakeLists.txt")

add_subdirectory(package/libpiksi)
add_subdirectory(package/zmq_adapter/src)
add_subdirectory(package/rotating_zmq_logger/src)
add_subdirectory(package/sbp_rtcm3_bridge/src)
add_subdirectory(package/libskylark/src)

# Testing
set (PACKAGE_BUILD_TESTS ON CACHE BOOL "Build the package tests")

if (PACKAGE_BUILD_TESTS)
  include("cmake/gtest.cmake")
  # Enable testing only works in root scope
  enable_testing ()
  add_subdirectory(host_tests/rotating_logger)
  # add_subdirectory(host_tests/sbp_rtcm3_bridge_tests)
  add_subdirectory(package/libskylark/tests/)
endif (PACKAGE_BUILD_TESTS)
