TARGET=libpiksi
SOURCES=\
  sbp_tx.c \
  sbp_rx.c \
  sbp_pubsub.c \
  settings.c \
  util.c \
  logging.c \
  networking.c \
  interface.c \
  runit.c \
  endpoint.c \
  loop.c \
  metrics.c \
  framer.c \
  filter.c \
  framer_none.c \
  filter_none.c \
  protocols.c \

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
