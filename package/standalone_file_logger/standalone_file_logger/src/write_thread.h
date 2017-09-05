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
#include <functional>

class RotatingLogger;

class WriteThread
{
  WriteThread() = delete;
  WriteThread(WriteThread const&) = delete;

public:
  typedef std::function<void(const uint8_t* data, size_t size)> WriteCall;
  typedef std::function<void(int, const char *)> LogCall;

  struct Callbacks {
    LogCall log_msg;
    WriteCall handle_write;
  };

  WriteThread(const Callbacks& callbacks);

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
  std::mutex _start_mutex;

  std::condition_variable _start_event;
  std::condition_variable _event;

  struct write_args {
    const uint8_t* data;
    size_t size;
  };

  write_args _queue[UINT8_MAX+1];
  Callbacks _callbacks;

  std::thread _thread;
};

#endif // SWIFTNAV_WRITE_THREAD_H
