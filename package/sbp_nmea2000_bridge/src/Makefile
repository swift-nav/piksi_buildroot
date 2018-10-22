TARGET=sbp_nmea2000_bridge
SOURCES= callbacks.cpp common.cpp device_and_product_info.cpp sbp_nmea2000_bridge.cpp sbp.cpp NMEA2000_SocketCAN.cpp SbpToNmea2kConverter.cpp
LIBS=-lnanomsg -lnmea2000 -lsbp -lpiksi -lsocketcan -lstdc++ -lm -luv
# TODO(lstrz): Remove -Wno-unused-function
# USE_N2K_CAN selects SocketCAN backend in Linux.
CFLAGS=-std=c++11 -DUSE_N2K_CAN=5 -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wcast-align -Wformat=2 -Wredundant-decls -Wformat-security -Wall -Wextra -Werror -Wno-format-nonliteral -Wno-unused-function -ggdb

CC=$(CROSS)g++

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
