cmake_minimum_required(VERSION 2.8.10)

project(sbp_rtcm3_bridge_tests CXX)

include_directories(${LIBSBP_RTCM3_BRIDGE_INCLUDE_DIRS} "${CZMQ_INCLUDE_DIRS}" "${LIBSBP_INCLUDE_DIRS}")

file(GLOB C_FILES *.c)

add_definitions(-std=gnu11)

add_executable(${PROJECT_NAME} ${C_FILES})

target_link_libraries(${PROJECT_NAME} sbp_rtcm3_bridge)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
)

add_test(${PROJECT_NAME} "${CMAKE_BINARY_DIR}/test/${PROJECT_NAME}" "${CMAKE_BINARY_DIR}/test")
