TARGET = endpoint_router

SOURCES = \
	endpoint_router.c \
	endpoint_router_load.c \
	endpoint_router_print.c \

OBJECTS = $(SOURCES:.c=.o)

LIBS=-luv -lnanomsg -lsbp -lpiksi -lyaml -lcmph

CFLAGS=-std=gnu11 -Wall

ARFLAGS=rcs $(LTO_PLUGIN)

CROSS=

CC=$(CROSS)gcc

all: program

program: $(TARGET) $(TARGET).a

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

$(TARGET).a: $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf $(TARGET)
