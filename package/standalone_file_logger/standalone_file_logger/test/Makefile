TARGET=test_standalone_file_logger
SOURCES= \
	run_rotating_logger_test.cc \
	../src/standalone_file_logger.a # should always be last
LIBS=-lczmq -lzmq -lsbp -lpiksi -lpthread -lgtest
CFLAGS=-std=gnu++11 -I../src

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
