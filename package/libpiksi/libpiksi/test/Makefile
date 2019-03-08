TARGET_TESTS = run_libpiksi_tests
TARGET_STR_TESTS = run_libpiksi_str_tests
TARGET_PK_TEST_SERVER	= pk_test_server

TARGETS =\
	$(TARGET_TESTS) \
	$(TARGET_STR_TESTS) \
	$(TARGET_PK_TEST_SERVER)

SOURCES_TESTS = \
	run_libpiksi_tests.cc \
	test_endpoint.cc \
	test_misc.cc \
	test_pubsub_loop_integration.cc \
	test_reqrep_loop_integration.cc \
	test_run.cc \
	test_sha256.cc \
	test_table.cc \
	test_version.cc \
	test_kmin.cc \

SOURCES_STR_TESTS = \
	test_str.cc

SOURCES_PK_TEST_SERVER = \
	pk_test_server.c

LIBS= \
	-luv -lpiksi -lsbp -ldl -lpthread -lsettings -lgtest

LIBS_TESTERS= \
	-luv -lpiksi -lsbp -ldl -lpthread -lsettings

CFLAGS=-std=gnu++11 -z muldefs -I. -I../include -L../src -ggdb3 $(PBR_CXX_WARNINGS)

CROSS=

CC=$(CROSS)g++

all: program
program: $(TARGETS)

$(TARGET_TESTS): $(SOURCES_TESTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TARGET_STR_TESTS): $(SOURCES_STR_TESTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TARGET_PK_TEST_SERVER): $(SOURCES_PK_TEST_SERVER)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS_TESTERS)

clean:
	rm -rf $(TARGET)
