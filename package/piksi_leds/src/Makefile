TARGET=piksi_leds
SOURCES= \
	firmware_state.c \
	led_adp8866.c \
	main.c \
	manage_led.c \

LIBS=-luv -lsbp -lpiksi -lpthread

CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

OBJS=$(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET)
