cmake_minimum_required(VERSION 2.8.10)

project(test_rotating_logger CXX)

set(LOGGER_DIR "../../package/rotating_zmq_logger/src")

include_directories(${GTEST_INCLUDE_DIR} ${LOGGER_DIR})

file(GLOB CC_FILES *.cc)
add_definitions(-std=gnu++11)

add_executable(${PROJECT_NAME} ${CC_FILES} "${LOGGER_DIR}/rotating_logger.cc")

target_link_libraries(${PROJECT_NAME} ${GTEST_LIBRARY} pthread)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
)

add_test(${PROJECT_NAME} "${CMAKE_BINARY_DIR}/test/test_rotating_logger" "${CMAKE_BINARY_DIR}/test")
