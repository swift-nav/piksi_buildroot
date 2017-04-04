TARGET=sbp_rtcm3_bridge
SOURCES= \
	sbp_rtcm3_bridge.c \
	sbp_rtcm3.c \
	rtcm3_decode.c \
	sbp.c

LIBS=-lczmq -lsbp -lpiksi -lm
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)