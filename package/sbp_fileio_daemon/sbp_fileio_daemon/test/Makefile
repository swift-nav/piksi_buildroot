TARGET=run_sbp_fileio_daemon_tests

SOURCES= \
	run_sbp_fileio_daemon_tests.cc \

LIBS= \
	-luv -lnanomsg -lpiksi -lsbp -ldl -lpthread -lgtest

CFLAGS=-std=gnu++11 -z muldefs -I. -I../include -L../src

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
