TARGET=sbp_settings_daemon
SOURCES= \
	main.c \
	minIni/minIni.c \
	sbp_zmq.c \
	sbp_fileio.c \
	settings.c \

LIBS=-lczmq -lsbp
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
