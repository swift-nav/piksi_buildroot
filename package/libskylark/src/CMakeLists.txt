cmake_minimum_required(VERSION 2.8.10)

project(skylark C)

add_definitions(-std=gnu11)

add_library(skylark libskylark.c libskylark.h)

include_directories("${CZMQ_INCLUDE_DIRS}"
  "${LIBSBP_INCLUDE_DIRS}"
  "${sbp_zmq_SOURCE_DIR}")

add_dependencies(${PROJECT_NAME} CZeroMQ libsbp sbp_zmq)
