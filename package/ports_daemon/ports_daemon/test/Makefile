TARGET=run_ports_daemon_test

SOURCES= \
	run_ports_daemon_test.cc

LIBS= \
	-luv -lnanomsg -lsbp -lpiksi -ldl -lpthread -lgtest -lstdc++ \
	../src/whitelists.o

CFLAGS=-std=gnu++11 -z muldefs -I../src

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
