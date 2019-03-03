TARGET=run_sbp_settings_daemon_test

SOURCES= \
	run_sbp_settings_daemon_test.cc

LIBS= \
	-luv -lsbp -lpiksi -ldl -lpthread -lsettings -lgtest \
	../src/setting.o

CFLAGS=-std=gnu++11 -z muldefs -I../include

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
