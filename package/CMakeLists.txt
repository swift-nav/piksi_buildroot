cmake_minimum_required(VERSION 2.8.7)
project(piksipackages)

SET(CMAKE_MODULE_PATH "cmake" )
SET(LIBZMQ_LIBRARIES "libzmq" )
SET(LIBZMQ_INCLUDE_DIRS "libzmq/include" )
SET(CZMQ_INCLUDE_DIRS "czmq/include" )

set(BUILD_SHARED_LIBS ON)
add_subdirectory(third_party/libsbp/c)
add_subdirectory(third_party/libzmq)
add_subdirectory(third_party/czmq)

add_subdirectory(sbp_protocol)
add_subdirectory(libpiksi)
add_subdirectory(zmq_adapter/src)
add_subdirectory(sbp_rtcm3_bridge/src)
