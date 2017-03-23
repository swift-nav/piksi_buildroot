TARGET=sbp_rtcm3_bridge
SOURCES= \
	sbp_rtcm3_bridge.c \
	sbp_rtcm3.c \
	sbp.c \
	rtcm3_decode.c

LIBS=-lczmq -lsbp -lm
CFLAGS=-std=gnu11 -fPIC
ARFLAGS=rcs
LDFLAGS=-shared

CROSS=

CC=$(CROSS)gcc

OBJS=$(SOURCES:.c=.o)

all: program
program: $(TARGET).a $(TARGET).so

$(OBJS): $(SOURCES)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET).so: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -rf $(TARGET).a $(TARGET).so $(OBJS)
