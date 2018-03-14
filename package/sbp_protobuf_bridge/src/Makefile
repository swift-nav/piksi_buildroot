TARGET=sbp_protobuf_bridge
SOURCES= main.c navigation.pb.c observation.pb.c gnss.pb.c obs_proto_converter.c
LIBS=-lczmq -lprotobuf-nanopb -lsbp -lpiksi
CFLAGS=-std=gnu11 -Wmissing-prototypes -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Werror -ggdb

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
