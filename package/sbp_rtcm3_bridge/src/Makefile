TARGET=sbp_rtcm3_bridge
SOURCES= \
	sbp_rtcm3_bridge.c \
	sbp_rtcm3.c \
	sbp.c \
	rtcm3_decode.c

LIBS=-lczmq -lsbp -lm
CFLAGS=-std=gnu11
INCLUDE=-I/home/pgrgich/piksi_buildroot/package/ -I/home/pgrgich/piksi_buildroot/buildroot/output/target/usr/include/

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(INCLUDE) $(LIBS)

clean:
	rm -rf $(TARGET)
