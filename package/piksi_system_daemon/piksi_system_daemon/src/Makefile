TARGET=piksi_system_daemon
SOURCES= \
	async-child.c \
	cellmodem.c \
	main.c \
	protocols.c \
	ports.c \
	ntrip.c \
	skylark.c \
	whitelists.c

LIBS=-lczmq -lsbp -lpiksi -ldl

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
	rm -rf $(TARGET)
