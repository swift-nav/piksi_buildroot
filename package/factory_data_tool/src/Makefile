TARGET=factory_data_tool
SOURCES= \
	factory_data_tool.c \
	uboot/factory_data.c
LIBS=-lz
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS) -I.

clean:
	rm -rf $(TARGET)
