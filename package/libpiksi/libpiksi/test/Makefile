TARGET=run_libpiksi_tests

SOURCES= \
	run_libpiksi_tests.cc \
	test_endpoint.cc \
	test_pubsub_loop_integration.cc

LIBS= \
	-luv -lczmq -lsbp -ldl -lpthread -lgtest \
	../src/libpiksi.so

CFLAGS=-std=gnu++11 -z muldefs -I.

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
