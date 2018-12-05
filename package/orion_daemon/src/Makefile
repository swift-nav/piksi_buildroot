TARGET=orion_daemon
SOURCES=orion.pb.cc orion.grpc.pb.cc orion_daemon.cc
LIBS=-luv -lsbp -lstdc++ -lprotobuf -lgrpc++_unsecure -lpthread -lpiksi -lsettings
CFLAGS+=-I. -std=c++14 -Werror -Wall

CROSS=

CC=$(CROSS)g++

OBJS=$(SOURCES:.cc=.o)

PROTOC="$(HOST_DIR)/bin/protoc"
GRPC_CPP_PLUGIN="$(HOST_DIR)/bin/grpc_cpp_plugin"

all: $(TARGET)

%.grpc.pb.cc: %.proto
	$(PROTOC) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) $<

%.pb.cc: %.proto
	$(PROTOC) --cpp_out=. $<

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

clean:
	rm -rf $(TARGET) $(OBJS)
