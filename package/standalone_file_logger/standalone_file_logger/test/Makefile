TARGET=test_standalone_file_logger
SOURCES= \
	run_rotating_logger_test.cc
LIBS= \
	-luv -lsbp -lpiksi -lpthread -lgtest -lsettings \
	../src/standalone_file_logger.a
CFLAGS=-std=gnu++11 -z muldefs -I../src

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
