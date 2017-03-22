TARGET=rotating_zmq_logger
SOURCES= \
	rotating_logger.cpp \
	rotating_zmq_logger.cpp
LIBS=-lczmq -lzmq
CFLAGS=-std=gnu++11

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
