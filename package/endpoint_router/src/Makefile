TARGET=endpoint_router
SOURCES= \
	endpoint_router.c \
	endpoint_router_load.c \
	endpoint_router_print.c
LIBS=-luv -lczmq -lsbp -lpiksi -lyaml
CFLAGS=-std=gnu11 -Wall

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
