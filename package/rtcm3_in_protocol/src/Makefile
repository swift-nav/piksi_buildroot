TARGET=librtcm3_in_protocol
SOURCES=info_rtcm3_in.c framer_rtcm3_in.c
CFLAGS+=-std=gnu11 -fPIC
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
