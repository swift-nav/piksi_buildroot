TARGET = endpoint_router

SOURCES = \
	endpoint_router.c \
	endpoint_router_load.c \
	endpoint_router_print.c \

LIBS=-luv -lnanomsg -lsbp -lpiksi -lyaml -lcmph

CFLAGS+=-O3 -ggdb3 -std=gnu11 -Wmissing-prototypes -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Wjump-misses-init -Werror -rdynamic -funwind-tables

ARFLAGS=rcs $(LTO_PLUGIN)

OBJECTS = $(SOURCES:.c=.o)

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
