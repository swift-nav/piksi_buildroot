cmake_minimum_required(VERSION 2.8.10)

project(skylark C)

include(FindCURL)

add_definitions(-std=gnu11)

add_library(skylark libskylark.c libskylark.h)

include_directories("${CZMQ_INCLUDE_DIRS}" "${LIBSBP_INCLUDE_DIRS}")

target_link_libraries(${PROJECT_NAME} piksi)
