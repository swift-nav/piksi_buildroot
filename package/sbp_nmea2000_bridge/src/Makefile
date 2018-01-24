TARGET=sbp_nmea2000_bridge
SOURCES= sbp_nmea2000_bridge.cpp sbp.cpp NMEA2000_SocketCAN.cpp
LIBS=-lczmq -lnmea2000 -lsbp -lpiksi -lsocketcan -lstdc++ -lm
CFLAGS=-std=c++11 -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wcast-align -Wformat=2 -Wredundant-decls -Wformat-security -Wall -Wextra -Werror -ggdb

CROSS=

CC=$(CROSS)g++

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
