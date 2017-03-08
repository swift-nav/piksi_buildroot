TARGET=skylark_download_daemon
SOURCES=skylark_download_daemon.c
LIBS=-lcurl  -lczmq -lsbp -lpiksi -lskylark
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
