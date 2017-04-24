TARGET=sbp_rtcm3_bridge
SOURCES= \
	sbp_rtcm3_bridge.c \
	sbp_rtcm3.c \
	rtcm3_decode.c \
	sbp.c

LIBS=-lczmq -lsbp -lpiksi -lm
CFLAGS=-std=gnu11
ARFLAGS=rcs

CROSS=

CC=$(CROSS)gcc

OBJS=$(SOURCES:.c=.o)

all: program
program: $(TARGET) $(TARGET).a

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf $(TARGET) $(TARGET).a $(OBJS)
