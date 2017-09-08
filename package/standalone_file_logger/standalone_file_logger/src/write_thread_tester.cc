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

  static char data1[] = "foobarbaz";

  auto write_func = [] (const uint8_t* data, size_t size) {
    
    assert( std::strcmp((char*)data, data1) == 0 );
//    printf("Callback 'write_func' done...\n");

    if (++count >= 255)
      pwrite_thread.load()->stop();
  };

  WriteThread write_thread;
  write_thread.set_callbacks(log_msg, write_func);

  pwrite_thread = &write_thread;

  for (int i = 0; i < 256; i++)
    write_thread.queue_data((uint8_t*)data1, sizeof(data1));

  write_thread.start();
  write_thread.queue_data((uint8_t*)data1, sizeof(data1));
  write_thread.join();

  pwrite_thread = nullptr;

  return true;
}

bool test_insert_remove_freq_same() {

  printf("\n\n<<<test_insert_remove_freq_same>>>\n\n");

  auto write_func = [] (const uint8_t* data, size_t size) {

    printf("Callback 'write_func' done (%zu)...\n", size);

    assert( size == sizeof(buffer_value.out) );
    
    int value = (int) *data;
    assert( value == buffer_value.out++ );

    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC));

    if (++count >= 100) {
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

    while (++write_count <= 100) {

      const size_t buflen = sizeof(buffer_value.in);
      char data1[buflen];

      memcpy(data1, &buffer_value.in, buflen);
      buffer_value.in++;

      write_thread.queue_data((uint8_t*)data1, buflen);
      std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC));
    }

    printf("test_insert_remove_freq_same: stopping driver thread...\n");
  };

  write_thread.start();
  auto write_thread_driver = std::thread(write_thread_func);

  write_thread.join();
  write_thread_driver.join();

  assert( write_thread.queue_empty() );
  assert( write_thread.alloc_bytes() == 0 );

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
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC*2));
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

  assert( write_thread.alloc_bytes() == 0 );

  return true;
}


int main() {

  //assert( test_basic() );
  //assert( test_insert_remove_freq_same() );
  //assert( test_teardown() );
  assert( test_stats_output() );
}
