TARGET=image_table_tool
SOURCES= \
	image_table_tool.c \
	uboot/image_table.c
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
