RESMON=resource_monitor
EXTRACE=extrace
TARGETS=$(RESMON) $(RESMON).a $(EXTRACE)
SOURCES=resource_monitor.c resource_query.c query_cpu_usage.c query_mem_usage.c sbp.c resmon_common.c query_sys_state.c query_sockets.c
LIBS=-luv -lsbp -lpiksi -lm
CFLAGS=-std=gnu11 -Wmissing-prototypes -Wconversion -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Werror -ggdb3
ARFLAGS=rcs $(LTO_PLUGIN)

CROSS=

CC=$(CROSS)gcc

OBJS=$(SOURCES:.c=.o)

all:	$(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(RESMON): $(OBJS)
	$(CC) $(CFLAGS) -o $(RESMON) $^ $(LIBS)

$(EXTRACE): extrace.c
	$(CC) -o $(EXTRACE) $^ $(LIBS)

$(RESMON).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf $(TARGETS)
