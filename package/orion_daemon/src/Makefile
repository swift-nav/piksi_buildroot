TARGET=orion_daemon
SOURCES=orion_daemon.c
LIBS=-luv -lnanomsg -lsbp -lpiksi
CFLAGS=

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
