TARGET=sbp_rtcm3_bridge
SOURCES= \
	sbp_rtcm3_bridge.c \
	rtcm3_decode.c \
	sbp.c

LIBS=-lczmq -lsbp
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
