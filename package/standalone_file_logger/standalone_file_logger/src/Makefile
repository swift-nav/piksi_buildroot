TARGET=standalone_file_logger
SOURCES= \
	rotating_logger.cc \
	standalone_file_logger.cc

LIBS=-luv -lsbp -lpiksi -lpthread
CFLAGS=-std=gnu++11 -ggdb3
ARFLAGS=rcs $(LTO_PLUGIN)

CROSS=

CC=$(CROSS)g++

OBJS=$(SOURCES:.cc=.o)

all: program
program: $(TARGET) $(TARGET).a

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf $(TARGET) $(TARGET).a $(OBJS)
