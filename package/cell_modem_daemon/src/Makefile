TARGET=cell_modem_daemon
SOURCES=\
	cell_modem_daemon.c \
	at_command_utils.c \
	cell_modem_settings.c \
	cell_modem_inotify.c \
	cell_modem_probe.c
LIBS=-lczmq -lsbp -lpiksi
CFLAGS=-std=gnu11 -Wmissing-prototypes -Wconversion -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Werror

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
