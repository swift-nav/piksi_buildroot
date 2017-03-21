TARGET=libpiksi
SOURCES=sbp_zmq_tx.c sbp_zmq_rx.c sbp_zmq_pubsub.c settings.c util.c
CFLAGS=-std=gnu11 -fPIC
ARFLAGS=rcs
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
