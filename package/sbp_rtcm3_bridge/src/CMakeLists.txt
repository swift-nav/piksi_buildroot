cmake_minimum_required(VERSION 2.8.10)

project(sbp_rtcm3_bridge C)

include_directories("${CZMQ_INCLUDE_DIRS}" "${LIBSBP_INCLUDE_DIRS}")

set(LIBSBP_RTCM3_BRIDGE_INCLUDE_DIRS ${PROJECT_SOURCE_DIR} PARENT_SCOPE)

add_definitions(-std=gnu11)

file(GLOB C_FILES *.c)

add_library(${PROJECT_NAME} STATIC ${C_FILES})

target_link_libraries(${PROJECT_NAME} czmq zmq sbp m)