cmake_minimum_required(VERSION 2.8.10)

project(test_skylark CXX)

include_directories("${GTEST_INCLUDE_DIRS}" "../src")
link_directories("${GTEST_LIBRARY_DIRS}")
file(GLOB CXX_FILES *.cc)
add_definitions(-std=gnu++11)

add_executable(${PROJECT_NAME} ${CXX_FILES} "../src/libskylark.c")
target_link_libraries(${PROJECT_NAME} gtest pthread)
add_dependencies(${PROJECT_NAME} googletest-distribution)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
)

add_test(${PROJECT_NAME}
  "${CMAKE_BINARY_DIR}/tests/test_skylark"
  "${CMAKE_BINARY_DIR}/tests")
