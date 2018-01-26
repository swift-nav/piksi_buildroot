TARGET=zmq_adapter
SOURCES= \
	zmq_adapter.c \
	zmq_adapter_stdio.c \
	zmq_adapter_file.c \
	zmq_adapter_tcp_listen.c \
	zmq_adapter_tcp_connect.c \
	zmq_adapter_udp_listen.c \
	zmq_adapter_udp_connect.c \
	framer.c \
	framer_none.c \
	filter.c \
	filter_none.c \
	filter_settings_whitelist.c \
	protocols.c \
	logging.c
LIBS=-lczmq -lzmq -lsbp -lpiksi -ldl -lcmph
CFLAGS += -std=gnu11 -Wall -Wextra -Wconversion

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET) $(TARGET).a

OBJS=$(SOURCES:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf $(TARGET)
