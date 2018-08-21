TARGET=skylark_daemon
SOURCES=skylark_daemon.c skylark_settings.c
LIBS=-luv -lcurl -lsbp -lpiksi -lnetwork
CFLAGS=-std=gnu11 -Wmissing-prototypes -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Werror

CROSS=

CC=$(CROSS)gcc

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
