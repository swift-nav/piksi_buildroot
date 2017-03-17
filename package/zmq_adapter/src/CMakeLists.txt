
include_directories("${CZMQ_INCLUDE_DIRS}" "${LIBZMQ_INCLUDE_DIRS}" "${LIBSBP_INCLUDE_DIRS}")
link_directories("${CZMQ_LIBRARY_DIRS}" "${LIBZMQ_LIBRARY_DIRS}" "${LIBSBP_LIBRARY_DIRS}")
add_executable(zmq_adapter zmq_adapter.c zmq_adapter_file.c zmq_adapter_tcp_listen.c	framer.c framer_none.c	framer_sbp.c	filter.c	filter_none.c	filter_sbp.c)

target_link_libraries(zmq_adapter czmq zmq sbp)
add_dependencies(zmq_adapter CZeroMQ libsbp)

set_target_properties( zmq_adapter
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/zmq_adapter/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/zmq_adapter/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/zmq_adapter/bin"
)