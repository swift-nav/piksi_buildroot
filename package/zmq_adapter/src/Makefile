TARGET=zmq_adapter
SOURCES= \
	zmq_adapter.c \
	zmq_adapter_file.c \
	zmq_adapter_tcp_listen.c \
	framer.c \
	framer_none.c \
	framer_sbp.c
LIBS=-lczmq -lsbp
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
