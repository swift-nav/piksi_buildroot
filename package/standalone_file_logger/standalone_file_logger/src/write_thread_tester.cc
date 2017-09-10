#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <chrono>
#include <thread>
#include <string>

#include <unistd.h>

#include "write_thread.h"

static std::atomic<WriteThread*> pwrite_thread;
static std::atomic<size_t> count;

struct { int in; int out; } buffer_value;

const unsigned THREAD_SLEEP_MSEC = 10;

void log_msg(int lvl, const char* msg) {
  printf("%s\n", msg);
}

bool test_basic() {

  printf("\n\n<<<test_basic>>>\n\n");

  static char data1[] = "foobarbazquuxqaz"; // 17 bytes
  size_t ncmp = 16;

  auto write_func = [&ncmp] (const uint8_t* data, size_t size) {

    assert( size == 4096 || size >= 16 );
    printf("%zu %zu\n", sizeof(data1), size);
    printf("'%s' '%s'\n", data, data1);

    assert( std::strncmp((char*)data, data1 + (16 - ncmp), ncmp) == 0 );
    printf("Callback 'write_func' done...\n");

    ncmp = ncmp == 16 ? 0 : ncmp+1;

    if (++count >= 16)
      pwrite_thread.load()->stop();
  };

  WriteThread write_thread;
  write_thread.set_callbacks(log_msg, write_func);

  pwrite_thread = &write_thread;

  for (int i = 0; i < (256*16); i++)
    write_thread.queue_data((uint8_t*)data1, sizeof(data1));

  write_thread.start();
  write_thread.queue_data((uint8_t*)data1, sizeof(data1));
  write_thread.join();

  pwrite_thread = nullptr;

  return true;
}


void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

bool test_insert_remove_freq_same() {

  printf("\n\n<<<test_insert_remove_freq_same>>>\n\n");

  const size_t buflen = sizeof(buffer_value.in);
  const int items_per_block = WriteThread::BLOCK_SIZE / buflen;
	const int block_write_count = 3;

  auto write_func = [&] (const uint8_t* data, size_t size) {

    printf("Callback 'write_func' done (%zu) (block write count: %zu)...\n", size, (size_t)count);

    assert( size == 4096 );

    for (int i = 0; i < items_per_block; i++) {
      
      int value = *(int*)data;
      printf("value = %d\n", value);
      assert( value == buffer_value.out++ );

      data += buflen;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC));

    if (++count >= block_write_count) {
      printf("test_insert_remove_freq_same: stopping...\n");
      pwrite_thread.load()->stop();
    }
  };

  count = 0;
  std::atomic<int> write_count(0);

  WriteThread write_thread;
  write_thread.set_callbacks(log_msg, write_func);

  pwrite_thread = &write_thread;

  auto write_thread_func = [&] () {

    printf("test_insert_remove_freq_same: starting driver thread...\n");

    uint8_t data1[buflen];
    while (++write_count <= block_write_count*items_per_block) {

      memcpy(data1, &buffer_value.in, buflen);
      buffer_value.in++;

      //printf("write_count = %zu\n", (size_t)write_count);

      write_thread.queue_data(data1, buflen);
      std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC));
    }

    printf("test_insert_remove_freq_same: stopping driver thread...\n");
  };

  write_thread.start();
  auto write_thread_driver = std::thread(write_thread_func);

  write_thread_driver.join();
  write_thread.join();

  assert( write_thread.queue_empty() );
  assert( write_thread.alloc_bytes() == WriteThread::BLOCK_SIZE );

  return true;
}

bool test_teardown() {
  
  printf("\n\n<<<test_teardown>>>\n\n");

  auto write_func = [] (const uint8_t* data, size_t size) {
  };

  size_t count = 100;

  while (count-- > 0) {

    WriteThread write_thread;
    write_thread.set_callbacks(log_msg, write_func);

    write_thread.start();
    write_thread.stop();
    write_thread.join();
  }

  return true;
}

bool test_stats_output() {
  
  using namespace std::chrono;

  printf("\n\n<<<test_stats_output>>>\n\n");

  static char data1[] = "foobarbaz";

  auto write_func = [] (const uint8_t* data, size_t size) {
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC*10));
  };

  size_t count = 100;

  time_point<steady_clock> start_time = steady_clock::now();

  auto seconds_count = [&] () -> size_t {
    return duration_cast<seconds>(steady_clock::now() - start_time).count();
  };

  WriteThread write_thread;
  write_thread.set_callbacks(log_msg, write_func);

  write_thread.start();

  while (seconds_count() < 30) {
    write_thread.queue_data((uint8_t*)data1, sizeof(data1));
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC));
  }

  write_thread.stop();
  write_thread.join();

  assert( write_thread.alloc_bytes() == WriteThread::BLOCK_SIZE );

  return true;
}

int main(int argc, const char* argv[]) {

	std::string target("all");

	if (argc > 1) {
		target = std::string(argv[1]);
	}

	if ( target == "test_basic" || target == "all" )
  	assert( test_basic() );

	if ( target == "test_insert_remove_freq_same" || target == "all" )
  	assert( test_insert_remove_freq_same() );

	if ( target == "test_teardown" || target == "all" )
  	assert( test_teardown() );

	if ( target == "test_stats_output" || target == "all" )
  	assert( test_stats_output() );

	return 0;
}
