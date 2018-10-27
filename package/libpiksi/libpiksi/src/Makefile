TARGET=libpiksi
SOURCES=$(notdir $(shell find . -name '*.c'))

ARFLAGS=rcs $(LTO_PLUGIN)
CFLAGS+=-std=gnu11 -fPIC -I../include -O3 -ggdb3
LDFLAGS+=-shared

CROSS=

CC=$(CROSS)gcc

OBJS=$(SOURCES:.c=.o)

all: program
program: $(TARGET).a $(TARGET).so

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET).so: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -rf $(TARGET).a $(TARGET).so $(OBJS)
