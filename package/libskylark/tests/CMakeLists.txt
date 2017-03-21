cmake_minimum_required(VERSION 2.8.10)

project(test_skylark CXX)

include_directories("${GTEST_INCLUDE_DIRS}" "../src")

# FIND_PACKAGE(libsbp_zmq)

MESSAGE( STATUS "UNIX: " ${libsbp_zmq_BINARY_DIR} )

link_directories("${GTEST_LIBRARY_DIRS}"
  "${CZMQ_LIBRARY_DIRS}"
  "${LIBSBP_LIBRARY_DIRS}"
  "${sbp_zmq_BINARY_DIR}")

include_directories("${CZMQ_INCLUDE_DIRS}"
  "${LIBSBP_INCLUDE_DIRS}"
  "${sbp_zmq_SOURCE_DIR}")

file(GLOB CXX_FILES *.cc)
add_definitions(-std=c++11)
add_executable(${PROJECT_NAME} ${CXX_FILES})

add_dependencies(${PROJECT_NAME} googletest-distribution)

target_link_libraries(${PROJECT_NAME}
  gtest pthread curl czmq sbp sbp_zmq skylark)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
)

add_test(${PROJECT_NAME}
  "${CMAKE_BINARY_DIR}/tests/test_skylark"
  "${CMAKE_BINARY_DIR}/tests")
