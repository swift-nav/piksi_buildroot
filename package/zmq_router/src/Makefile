TARGET=zmq_router
SOURCES= \
	zmq_router.c \
	zmq_router_load.c \
	zmq_router_print.c
LIBS=-lczmq -lyaml
CFLAGS=-std=gnu11 -Wall

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
