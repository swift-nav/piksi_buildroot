cmake_minimum_required(VERSION 2.8.10)

project(rotating_zmq_logger CXX)

include_directories("${CZMQ_INCLUDE_DIRS}")
include_directories("${LIBSBP_INCLUDE_DIRS}")

file(GLOB CC_FILES *.cc)
add_definitions(-std=gnu++11)

add_executable(${PROJECT_NAME} ${CC_FILES})

target_link_libraries(${PROJECT_NAME} czmq zmq piksi)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
