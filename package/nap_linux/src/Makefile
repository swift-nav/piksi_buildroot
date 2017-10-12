TARGET  := nap_linux
SOURCES := nap_linux.cc
CFLAGS  := $(CFLAGS) -I. -Wall -std=c++11

nap_linux.cc: obfuscated_string.h \
						  nap_linux.h \
							registers/swiftnap.h \
							libswiftnav/common.h

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CXX) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
