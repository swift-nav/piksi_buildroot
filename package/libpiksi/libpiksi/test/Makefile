TARGET=run_libpiksi_tests
TARGET_STR_TESTS=run_libpiksi_str_tests

TARGETS=$(TARGET) $(TARGET_STR_TESTS)

SOURCES=$(filter-out test_str.cc,$(notdir $(shell find . -name '*.cc')))
SOURCES_STR_TESTS=test_str.cc

LIBS= \
	-luv -lnanomsg -lpiksi -lsbp -ldl -lpthread -lgtest

CFLAGS=-std=gnu++11 -z muldefs -I. -I../include -L../src -ggdb3

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGETS)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TARGET_STR_TESTS): $(SOURCES_STR_TESTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf $(TARGET)
