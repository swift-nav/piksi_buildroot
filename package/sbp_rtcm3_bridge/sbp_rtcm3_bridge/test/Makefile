TARGET=test_sbp_rtcm3_bridge
SOURCES= \
	rtcm_decoder_tests.c \
	sbp_rtcm_converter_tests.c \
	../src/sbp_rtcm3_bridge.a # should always be last
LIBS=-lczmq -lsbp -lpiksi -lm
CFLAGS=-std=gnu11 -I../src

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
