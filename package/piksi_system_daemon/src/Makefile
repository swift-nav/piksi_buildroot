TARGET=piksi_system_daemon
SOURCES= \
	main.c \
	protocols.c \
	ports.c \
	ntrip.c \
	skylark.c \
	whitelists.c

LIBS=-lczmq -lsbp -lpiksi -ldl
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
