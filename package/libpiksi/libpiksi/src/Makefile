TARGET=libpiksi
SOURCES=\
  sbp_zmq_tx.c \
  sbp_zmq_rx.c \
  sbp_zmq_pubsub.c \
  settings.c \
  util.c \
  logging.c \
  networking.c \
  interface.c \
  runit.c \
  sbp_pubsub.c \
  endpoint.c \
  sbp_tx.c \
  sbp_rx.c \
  loop.c
ARFLAGS=rcs $(LTO_PLUGIN)
CFLAGS+=-std=gnu11 -fPIC -I../include
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
