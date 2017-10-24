TARGET  := nap_linux_c
SOURCES := nap_linux_c.c

CFLAGS  := $(CFLAGS) -I. -Wall -std=gnu99

nap_linux.cc: nap_linux.h \
							registers/swiftnap.h \
							libswiftnav/common.h

all:	$(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
