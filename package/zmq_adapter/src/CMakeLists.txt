cmake_minimum_required(VERSION 2.8.10)

project(zmq_adapter C)

include_directories("${CZMQ_INCLUDE_DIRS}" "${LIBSBP_INCLUDE_DIRS}")

file(GLOB C_FILES *.c)

add_executable(${PROJECT_NAME} ${C_FILES})

target_link_libraries(${PROJECT_NAME} czmq zmq sbp)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
