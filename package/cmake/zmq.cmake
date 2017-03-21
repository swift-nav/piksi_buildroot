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
  # We use SOURCE_DIR because we use version control to track the
  # version of this library instead of using the build tool
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/libzmq
  INSTALL_DIR ${ZMQ_INSTALL}
  #INSTALL_COMMAND cmake -E echo "Not installing ZeroMQ globally."
  CMAKE_ARGS -DZMQ_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${ZMQ_INSTALL}
  # This simply passes down cmake arguments, which allows us to define
  # zmq-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})
set(LIBZMQ_INCLUDE_DIRS ${ZMQ_INSTALL}/include)
set(LIBZMQ_LIBRARY_DIRS ${ZMQ_INSTALL}/lib)

# build directory
set(CZMQ_PREFIX ${CMAKE_BINARY_DIR}/external/CZMQ-prefix)
# install directory
set(CZMQ_INSTALL ${CMAKE_BINARY_DIR}/external/CZMQ-install)
# This adds czmq as an external project with the specified parameters.
ExternalProject_Add(CZeroMQ
  PREFIX ${CZMQ_PREFIX}
  # We use SOURCE_DIR because we use version control to track the
  # version of this library instead of using the build tool
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/czmq
  INSTALL_DIR ${CZMQ_INSTALL}
  # TODO (mookerji, jdiamond): resolve
  CMAKE_ARGS -DLIBZMQ_INCLUDE_DIRS=${LIBZMQ_INCLUDE_DIRS} -DCMAKE_INSTALL_PREFIX=${CZMQ_INSTALL} -DLIBZMQ_LIBRARIES=${LIBZMQ_LIBRARY_DIRS}/libzmq.dylib
  # This simply passes down cmake arguments, which allows us to define
  # zmq-specific cmake flags as arguments to the toplevel cmake
  # invocation.
  CMAKE_ARGS ${CMAKE_ARGS})
set(CZMQ_INCLUDE_DIRS ${CZMQ_INSTALL}/include ${LIBZMQ_INCLUDE_DIRS})
set(CZMQ_LIBRARY_DIRS ${CZMQ_INSTALL}/lib ${LIBZMQ_LIBRARY_DIRS})
add_dependencies(CZeroMQ ZeroMQ)
