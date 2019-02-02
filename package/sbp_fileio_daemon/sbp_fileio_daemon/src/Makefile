TARGET_DAEMON=sbp_fileio_daemon
DAEMON_SOURCES= \
	main.c \
	sbp_fileio.c \
	path_validator.c \

TARGET_FLUSH=sbp_fileio_flush
FLUSH_SOURCES= \
	flush_main.c \
	sbp_fileio.c \
	path_validator.c \

TARGETS=$(TARGET_DAEMON) $(TARGET_FLUSH)
ARCHIVE_TARGETS=$(TARGET_DAEMON).a $(TARGET_FLUSH).a

LIBS=-luv -lsbp -lpiksi -lsettings -lpthread

ARFLAGS=rcs $(LTO_PLUGIN)

CFLAGS=-ggdb3 -std=gnu11 -Wmissing-prototypes -Wimplicit -Wshadow -Wswitch-default -Wswitch-enum -Wundef -Wuninitialized -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wformat=2 -Wimplicit-function-declaration -Wredundant-decls -Wformat-security -Wall -Wextra -Wno-strict-prototypes -Wjump-misses-init -Werror -fPIC -I.

CROSS=

DAEMON_OBJS=$(DAEMON_SOURCES:.c=.o)
FLUSH_OBJS=$(FLUSH_SOURCES:.c=.o)

ALL_OBJS=$(DAEMON_OBJS) $(FLUSH_OBJS)

CC=$(CROSS)gcc

all: $(ARCHIVE_TARGETS) $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_DAEMON).a: $(DAEMON_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET_FLUSH).a: $(FLUSH_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET_DAEMON): $(DAEMON_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET_DAEMON) $(DAEMON_OBJS) $(LIBS)

$(TARGET_FLUSH): $(FLUSH_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET_FLUSH) $(FLUSH_OBJS) $(LIBS)

clean:
	rm -rf $(ARCHIVE_TARGETS) $(ALL_OBJS) $(TARGETS)
