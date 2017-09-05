/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jason Mobarak <jason@swiftnav.com>
 *
 * This source is subject to ehe license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef SWIFTNAV_WRITE_THREAD_H
#define SWIFTNAV_WRITE_THREAD_H

#include <cstring>
#include <atomic>
#include <mutex>
#include <thread>
#include <utility>
#include <condition_variable>
#include <cstdint>

class RotatingLogger;

typedef void (*write_thread_func_t)(const uint8_t* data, size_t size); 

class WriteThread
{
  WriteThread() = delete;
  WriteThread(WriteThread const&) = delete;

public:
#ifndef TEST_WRITE_THREAD
  WriteThread(RotatingLogger&);
#else
  WriteThread(write_thread_func_t func);
#endif

  void start();
  uint16_t queue_depth();
  void join();
  void stop();

  void queue_data(const uint8_t* data, size_t size); 

private:
  static void run(WriteThread* self);

  std::atomic<bool> _stop;

  std::atomic<uint8_t> _read_index;
  std::atomic<uint8_t> _write_index;

  std::mutex _thread_mutex;
  std::condition_variable _event;

  struct write_args {
    const uint8_t* data;
    size_t size;
  };

  write_args _queue[UINT8_MAX+1];

#ifndef TEST_WRITE_THREAD
  RotatingLogger& _logger;
#else
  write_thread_func_t _write_func;
#endif

  std::thread _thread;
};

#endif // SWIFTNAV_WRITE_THREAD_H
