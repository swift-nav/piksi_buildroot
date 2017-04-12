TARGET=rotating_zmq_logger
SOURCES= \
	rotating_logger.cc \
	rotating_zmq_logger.cc
LIBS=-lczmq -lzmq -lsbp -lpiksi
CFLAGS=-std=gnu++11

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
