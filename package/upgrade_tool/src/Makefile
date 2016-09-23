TARGET=upgrade_tool
SOURCES= \
	upgrade_tool.c \
	singleton.c \
	partition_info.c \
	upgrade_data.c \
	mtd.c \
	factory_params.c \
	uboot/image_table.c \
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
