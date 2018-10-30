TARGET=sbp_fileio_daemon
SOURCES= \
	main.c \
	sbp_fileio.c \
	path_validator.c \

LIBS=-luv -lsbp -lpiksi

ARFLAGS=rcs $(LTO_PLUGIN)

CFLAGS=-std=gnu11 -Wmissing-prototypes -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Wjump-misses-init -Werror -fPIC -I.

CROSS=

OBJS=$(SOURCES:.c=.o)

CC=$(CROSS)gcc

all: program
	
program: $(TARGET).a $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

clean:
	rm -rf $(TARGET).a $(TARGET) $(OBJS)
