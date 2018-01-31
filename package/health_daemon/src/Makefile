TARGET=health_daemon
SOURCES=health_daemon.c health_monitor.c baseline_monitor.c glo_bias_monitor.c glo_obs_monitor.c utils.c glo_health_context.c
LIBS=-lczmq -lsbp -lpiksi -lm
CFLAGS=-std=gnu11 -Wmissing-prototypes -Wconversion -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Werror

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
