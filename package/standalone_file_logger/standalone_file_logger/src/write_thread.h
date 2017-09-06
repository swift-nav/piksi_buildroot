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
#include <deque>

class RotatingLogger;

class WriteThread
{
  WriteThread(WriteThread const&) = delete;

public:
  static const int MAX_ALLOC = 2 * 1024 * 1024; // 2 megabytes

  typedef std::function<void(const uint8_t* data, size_t size)> WriteCall;
  typedef std::function<void(int, const char *)> LogCall;

  WriteThread();

  void start();
  void join();
  void stop();

  bool queue_empty();
  void queue_data(const uint8_t* data, size_t size); 

  void set_callbacks(const LogCall& log_call, const WriteCall& handle_write);

private:
  static void run(WriteThread* self);
  void _queue_data(const uint8_t* data, size_t size, bool stop); 

  struct write_args {
    bool stop;
    const uint8_t* data;
    size_t size;
  };

  LogCall _log_msg;
  WriteCall _handle_write;

  std::condition_variable _start_event;
  std::mutex _start_mutex;

  std::condition_variable _new_data_event;
  std::mutex _new_data_mutex;

  std::mutex _thread_mutex;

  std::deque<write_args> _queue;
  std::atomic<size_t> _alloc_bytes;

  std::thread _thread;
};

#endif // SWIFTNAV_WRITE_THREAD_H
