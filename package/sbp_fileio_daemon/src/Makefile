TARGET=sbp_fileio_daemon
SOURCES= \
	main.c \
	sbp_fileio.c

LIBS=-luv -lnanomsg -lsbp -lpiksi

CFLAGS=-std=gnu11 -Wmissing-prototypes -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Wjump-misses-init -Werror

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
