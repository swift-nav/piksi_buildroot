TARGET=libanpp_protocol
SOURCES=info_anpp.c framer_anpp.c an_packet_protocol.c
CFLAGS+=-std=gnu11 -fPIC -ggdb3
ARFLAGS=rcs $(LTO_PLUGIN)
LDFLAGS=-shared

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
