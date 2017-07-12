TARGET=test_sbp_rtcm3_bridge
SOURCES= \
	rtcm_decoder_tests.c \
	sbp_rtcm_converter_tests.c
LIBS= \
	-lczmq -lsbp -lpiksi -lm \
	../src/sbp_rtcm3_bridge.a
CFLAGS=-std=gnu11 -z muldefs -I../src

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
