RESMON=resource_monitor
EXTRACE=extrace
TARGETS=$(RESMON) $(EXTRACE)
SOURCES=resource_monitor.c resource_query.c query_cpu_usage.c query_mem_usage.c sbp.c resmon_common.c query_extrace.c
LIBS=-luv -lnanomsg -lsbp -lpiksi -lm
CFLAGS=-std=gnu11 -Wmissing-prototypes -Wconversion -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Werror

CROSS=

CC=$(CROSS)gcc

all:	$(TARGETS)

$(RESMON): $(SOURCES)
	$(CC) $(CFLAGS) -o $(RESMON) $^ $(LIBS)

$(EXTRACE): extrace.c
	$(CC) -o $(EXTRACE) $^

clean:
	rm -rf $(TARGETS)
