TARGET=piksi_sample
SOURCES=main.c
LIBS= -lczmq -lsbp -lpiksi
CFLAGS= -std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET):	$(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
