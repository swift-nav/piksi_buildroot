cmake_minimum_required(VERSION 2.8.10)

project(sbp_zmq C)

include_directories("${CZMQ_INCLUDE_DIRS}" "${LIBSBP_INCLUDE_DIRS}")

add_library(sbp_zmq sbp_zmq.c sbp_zmq.h)

add_dependencies(${PROJECT_NAME} CZeroMQ libsbp)
