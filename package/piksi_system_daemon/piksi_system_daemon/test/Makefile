TARGET=run_piksi_system_daemon_test

SOURCES= \
	run_piksi_system_daemon_test.cc

LIBS= \
	-lczmq -lsbp -lpiksi -ldl -lpthread -lgtest \
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
