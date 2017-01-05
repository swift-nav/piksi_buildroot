TARGET=zmq_router
SOURCES=zmq_router.c zmq_router_sbp.c zmq_router_nmea.c
LIBS=-lczmq
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
