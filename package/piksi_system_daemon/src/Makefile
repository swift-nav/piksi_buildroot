TARGET=piksi_system_daemon
SOURCES= \
	main.c \
	whitelists.c

LIBS=-lczmq -lsbp -lsbp_zmq -lsbp_settings
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
