cmake_minimum_required(VERSION 2.8)

project(libpiksi C)

include_directories("${CZMQ_INCLUDE_DIRS}")
include_directories("${LIBSBP_INCLUDE_DIRS}")

add_definitions(-std=gnu11)

file(GLOB C_FILES libpiksi/src/*.c)
file(GLOB H_FILES libpiksi/include/libpiksi/*.h)

add_library(piksi STATIC ${C_FILES} ${H_FILES})

target_link_libraries(piksi zmq czmq sbp)
target_include_directories(piksi PUBLIC libpiksi/include)
