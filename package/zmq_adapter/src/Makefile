TARGET=zmq_adapter
SOURCES= \
	zmq_adapter.c \
	zmq_adapter_stdio.c \
	zmq_adapter_file.c \
	zmq_adapter_tcp_listen.c \
	zmq_adapter_tcp_connect.c \
	zmq_adapter_udp_listen.c \
	zmq_adapter_udp_connect.c \
	framer.c \
	framer_none.c \
	filter.c \
	filter_none.c \
	protocols.c
LIBS=-lczmq -lzmq -lsbp -ldl
CFLAGS=-std=gnu11 -Wall

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
