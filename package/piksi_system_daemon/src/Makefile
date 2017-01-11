TARGET=piksi_system_daemon
SOURCES= \
	main.c \
	sbp_zmq.c \
	settings.c \
	whitelists.c \

LIBS=-lczmq -lsbp
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
