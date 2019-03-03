TARGET=sbp_settings_daemon
SOURCES= \
	main.c \
	setting.c \
	settings_sbp_cb.c \

LIBS=-luv -lsbp -lpiksi -lsettings

CFLAGS+=-std=gnu11 -I../include
ARFLAGS=rcs	$(LTO_PLUGIN)

CROSS=

CC=$(CROSS)gcc

OBJS=$(SOURCES:.c=.o)

all: program
program: $(TARGET) $(TARGET).a

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(TARGET).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf $(TARGET)
