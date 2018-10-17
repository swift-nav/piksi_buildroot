TARGET=run_metrics_daemon_test

SOURCES= \
	run_metrics_daemon_test.cc

LIBS= \
	-ljson-c -luv -lnanomsg -lsbp -lpiksi -ldl -lpthread -lgtest \
	../src/metrics_daemon.o\
	../src/sbp.o

CFLAGS=-std=gnu++11 -z muldefs -I../src

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
