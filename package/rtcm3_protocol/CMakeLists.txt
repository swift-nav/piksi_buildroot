cmake_minimum_required(VERSION 2.8)

project(rtcm3_protocol C)

add_definitions(-std=gnu11)

file(GLOB C_FILES src/*.c)

add_library(rtcm3_protocol SHARED ${C_FILES})

set_target_properties(rtcm3_protocol
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/usr/lib/zmq_protocols"
)
