TARGET=ota_daemon
SRC=$(notdir $(shell find . -name '*.c'))
LIBS=-luv -lcurl -lsbp -lpiksi -lnetwork -lm -ljson-c
CFLAGS=-std=gnu11 -Wmissing-prototypes -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Werror

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -rf $(TARGET)
