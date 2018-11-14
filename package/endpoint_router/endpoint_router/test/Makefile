TARGET=test_endpoint_router

SOURCES= \
	endpoint_router_test.cc

LIBS= \
	-luv -lsbp -lpiksi -lgtest -ldl -lyaml -lpthread \
	../src/endpoint_router.a

CFLAGS=-std=gnu++11 -z muldefs -I../src -g

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES) ../src/endpoint_router.a
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
