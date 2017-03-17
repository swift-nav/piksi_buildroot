cmake_minimum_required(VERSION 2.8.7)
project(piksipackages)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("cmake/zmq.cmake")
include("cmake/sbp.cmake")

# set(BUILD_SHARED_LIBS ON)
# add_subdirectory(third_party/libsbp/c)
# add_subdirectory(third_party/libzmq)
# add_subdirectory(third_party/czmq)

add_subdirectory(zmq_adapter/src)


# include(cmake/SanitizeTargets.cmake)
# add_subdirectory(src)
# add_subdirectory(docs)
# add_subdirectory(tests)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")
# if (NOT CMAKE_CROSSCOMPILING)
#   add_subdirectory(third_party/googletest)
#   add_subdirectory(third_party/rapidcheck)
#   add_subdirectory(third_party/rapidcheck/extras/gtest)
# endif ()
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
# add_subdirectory(cpp_tests)
