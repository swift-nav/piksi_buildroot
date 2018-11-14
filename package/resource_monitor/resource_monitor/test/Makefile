TARGET=run_resource_monitor_test

SOURCES= \
	run_resource_monitor_test.cc

RESMON_A = ../src/resource_monitor.a

LIBS= \
	-ljson-c -luv -lsbp -lpiksi -ldl -lpthread -lgtest \
	$(RESMON_A)

CFLAGS=-std=gnu++11 -z muldefs -I../src

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES) $(RESMON_A)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
