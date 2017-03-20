cmake_minimum_required(VERSION 2.8.10)

project(libskylark CXX)

include_directories("${CZMQ_INCLUDE_DIRS}")
link_directories("${CZMQ_LIBRARY_DIRS}")

file(GLOB CXX_FILES *.c)
add_definitions(-std=gnu11)

add_executable(${PROJECT_NAME} ${CXX_FILES})

target_link_libraries(${PROJECT_NAME} czmq zmq)

add_dependencies(${PROJECT_NAME} CZeroMQ)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
