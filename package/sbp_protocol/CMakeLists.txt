cmake_minimum_required(VERSION 2.8)

project(sbp_protocol C)

include_directories("${LIBSBP_INCLUDE_DIRS}")

add_definitions(-std=gnu11)

file(GLOB C_FILES src/*.c)

add_library(sbp_protocol SHARED ${C_FILES})

target_link_libraries(sbp_protocol sbp)

set_target_properties(sbp_protocol
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/usr/lib/zmq_protocols"
)
