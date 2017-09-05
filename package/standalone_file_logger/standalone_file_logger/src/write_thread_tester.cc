#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <chrono>
#include <thread>

#include <unistd.h>

#include "write_thread.h"

static std::atomic<WriteThread*> pwrite_thread;
static std::atomic<size_t> count;

struct {
  int in;
  int out;
} buffer_value;

const unsigned THREAD_SLEEP_MSEC = 10;

bool test_insert_remove_freq_same() {

  auto write_func = [] (const uint8_t* data, size_t size) {

    printf("Callback 'write_func' done (%zu)...\n", size);

    assert( size == sizeof(buffer_value.out) );
    
    int value = (int) *data;
    delete[] data;

    assert( value == buffer_value.out++ );

    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC));

    if (++count >= 100) {
      printf("test_insert_remove_freq_same: stopping...\n");
      pwrite_thread.load()->stop();
    }
  };

  count = 0;
  int write_count = 0;

  WriteThread write_thread(write_func);
  pwrite_thread = &write_thread;

  auto write_thread_func = [&] () {

    while (++write_count <= 100) {

      size_t buflen = sizeof(buffer_value.in);
      char* data1 = new char[buflen];

      memcpy(data1, &buffer_value.in, buflen);
      buffer_value.in++;

      write_thread.queue_data((uint8_t*)data1, buflen);
      std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP_MSEC));
    }
  };

  write_thread.start();
  auto write_thread_driver = std::thread(write_thread_func);

  write_thread.join();
  write_thread_driver.join();

  assert( write_thread.queue_depth() == 0 );

  return true;
}

bool test_basic() {

  static char data1[] = "foobarbaz";

  auto write_func = [] (const uint8_t* data, size_t size) {
    
    assert( std::strcmp((char*)data, data1) == 0 );
//    printf("Callback 'write_func' done...\n");

    if (++count >= 255)
      pwrite_thread.load()->stop();
  };

  WriteThread write_thread(write_func);
  pwrite_thread = &write_thread;

  for (int i = 0; i < 256; i++)
    write_thread.queue_data((uint8_t*)data1, sizeof(data1));

  write_thread.start();
  write_thread.queue_data((uint8_t*)data1, sizeof(data1));
  write_thread.join();

  pwrite_thread = nullptr;

  return true;
}

int main() {

  assert( test_basic() );
  assert( test_insert_remove_freq_same() );
}
