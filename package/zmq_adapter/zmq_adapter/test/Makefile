TARGET=test_zmq_adapter
SOURCES= \
	zmq_adapter_test.cc
LIBS= \
	-lczmq -lzmq -lsbp -lpiksi -lpthread -lgtest -ldl -lcmph \
	../src/zmq_adapter.a
CFLAGS=-std=gnu++11 -z muldefs -I../src

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES) ../src/zmq_adapter.a
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
