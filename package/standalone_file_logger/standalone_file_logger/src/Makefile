TARGET=standalone_file_logger
SOURCES= \
	rotating_logger.cc \
	standalone_file_logger.cc \
  write_thread.cc

LIBS=-lczmq -lzmq -lsbp -lpiksi -lpthread -latomic
CFLAGS=-std=gnu++11 -g
ARFLAGS=rcs

CROSS=

CC=$(CROSS)g++

OBJS=$(SOURCES:.cc=.o)

all: program
program: $(TARGET) $(TARGET).a

test-write_thread: write_thread_tester
	@echo "Running write_thread_tester..."
	@./write_thread_tester

write_thread_tester: write_thread.cc write_thread_tester.cc
	g++ $(CFLAGS) -g -DTEST_WRITE_THREAD=1 $^ -o $@

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf $(TARGET) $(TARGET).a $(OBJS)
