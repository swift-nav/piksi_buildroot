cmake_minimum_required(VERSION 2.8)

# This brings in the external project support in cmake
include(ExternalProject)

# build directory
set(ZMQ_PREFIX ${CMAKE_BINARY_DIR}/external/ZMQ-prefix)
# install directory
set(ZMQ_INSTALL ${CMAKE_BINARY_DIR}/external/ZMQ-install)
# This adds zmq as an external project with the specified parameters.
ExternalProject_Add(ZeroMQ
  PREFIX ${ZMQ_PREFIX}
  # checks out version that matches buildroot/package/zeromq/zeromq.mk
  # release download doesn't build in cmake so using repo
  GIT_REPOSITORY https://github.com/zeromq/zeromq4-1
  GIT_TAG v4.1.5
  INSTALL_DIR ${ZMQ_INSTALL}
  #INSTALL_COMMAND cmake -E echo "Not installing ZeroMQ globally."
  CMAKE_ARGS -DZMQ_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${ZMQ_INSTALL}
  # This simply passes down cmake arguments, which allows us to define
  # zmq-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})
set(LIBZMQ_INCLUDE_DIRS ${ZMQ_INSTALL}/include)
set(LIBZMQ_LIBRARY_DIR ${ZMQ_INSTALL}/lib)

set(ZMQ_LIBRARY_PATH ${LIBZMQ_LIBRARY_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}zmq${CMAKE_SHARED_LIBRARY_SUFFIX})
set(ZMQ_LIBRARY zmq)
add_library(${ZMQ_LIBRARY} SHARED IMPORTED)
set_property(TARGET ${ZMQ_LIBRARY} PROPERTY IMPORTED_LOCATION
                ${ZMQ_LIBRARY_PATH} )
add_dependencies(${ZMQ_LIBRARY} ZeroMQ)

set(ZMQ_STATIC_LIBRARY_PATH ${LIBZMQ_LIBRARY_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}zmq-static${CMAKE_STATIC_LIBRARY_SUFFIX})
set(ZMQ_STATIC_LIBRARY zmq-static)
add_library(${ZMQ_STATIC_LIBRARY} STATIC IMPORTED)
set_property(TARGET ${ZMQ_STATIC_LIBRARY} PROPERTY IMPORTED_LOCATION
                ${ZMQ_STATIC_LIBRARY_PATH} )
add_dependencies(${ZMQ_STATIC_LIBRARY} ZeroMQ)


# build directory
set(CZMQ_PREFIX ${CMAKE_BINARY_DIR}/external/CZMQ-prefix)
# install directory
set(CZMQ_INSTALL ${CMAKE_BINARY_DIR}/external/CZMQ-install)
# This adds czmq as an external project with the specified parameters.
ExternalProject_Add(CZeroMQ
  PREFIX ${CZMQ_PREFIX}
  #checks out version that matches buildroot/package/czmq/czmq.mk
  GIT_REPOSITORY https://github.com/zeromq/czmq
  GIT_TAG 5205ec201e97c3a652c17eb86b18b70350b54512
  INSTALL_DIR ${CZMQ_INSTALL}
  # Pass the ZMQ library and header locations to czmq's FindLibZMQ cmake
  # file. It seems that CZMQ on some Linux systems (i.e., Travis) ignore
  # explicit paths and rely on HINTS to find a project-local libzmq. Developer
  # machines are exactly the opposite. For the moment, we pass both paths here
  # to satisfy integration requirements on Travis and local use.
  CMAKE_ARGS -DLIBZMQ_INCLUDE_DIRS=${LIBZMQ_INCLUDE_DIRS} -DCMAKE_INSTALL_PREFIX=${CZMQ_INSTALL} -DLIBZMQ_LIBRARIES=${ZMQ_LIBRARY_PATH}
  # This simply passes down cmake arguments, which allows us to define
  # zmq-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})

set(CZMQ_INCLUDE_DIRS ${CZMQ_INSTALL}/include ${LIBZMQ_INCLUDE_DIRS})
set(CZMQ_LIBRARY_DIR ${CZMQ_INSTALL}/lib)
add_dependencies(CZeroMQ ZeroMQ)

set(CZMQ_LIBRARY_PATH ${CZMQ_LIBRARY_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}czmq${CMAKE_SHARED_LIBRARY_SUFFIX})
set(CZMQ_LIBRARY czmq)
add_library(${CZMQ_LIBRARY} SHARED IMPORTED)
set_property(TARGET ${CZMQ_LIBRARY} PROPERTY IMPORTED_LOCATION
                ${CZMQ_LIBRARY_PATH} )
add_dependencies(${CZMQ_LIBRARY} CZeroMQ)
