


include_directories("../../third_party/libsbp/c/include/")

add_executable(zmq_adapter zmq_adapter.c zmq_adapter_file.c zmq_adapter_tcp_listen.c	framer.c framer_none.c	framer_sbp.c	filter.c	filter_none.c	filter_sbp.c)

target_link_libraries(zmq_adapter czmq zmq sbp)