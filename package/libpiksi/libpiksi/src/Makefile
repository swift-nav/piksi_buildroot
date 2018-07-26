TARGET=libpiksi
SOURCES=$(notdir $(shell find . -name '*.c'))

ARFLAGS=rcs $(LTO_PLUGIN)
CFLAGS+=-std=gnu11 -fPIC -I../include -ggdb3 -rdynamic -funwind-tables -O3
LDFLAGS+=-shared -rdynamic

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
