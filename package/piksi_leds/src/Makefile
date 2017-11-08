TARGET=piksi_leds
SOURCES= \
	main.c \

LIBS=-lczmq -lsbp -lpiksi

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
