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
#include <queue>

class RotatingLogger;

class WriteThread
{
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

  WriteThread(WriteThread const&) = delete;

public:
  typedef std::function<void(const uint8_t* data, size_t size)> WriteCall;
  typedef std::function<void(int, const char *)> LogCall;

  WriteThread();
  ~WriteThread();

  void start();
  void join();
  void stop();

  bool queue_empty();
  bool alloc_bytes();

  void queue_data(const uint8_t* data, size_t size); 

  void set_callbacks(const LogCall& log_call, const WriteCall& handle_write);

private:

  size_t msecs_since_last_stats();
  size_t seconds_since_last_warn();

  void update_buffer_stats(size_t size);
  void update_input_bps(size_t size);
  void update_output_bps(size_t size, TimePoint& write_start_time);

  static void update_bps_impl(size_t size,
                              TimePoint& last_time,
                              std::queue<double>& bps_value_queue,
                              double& average_bps,
                              bool update_last_time = false);

  void queue_data_impl(const uint8_t* data, size_t size, bool stop); 
  void free_queue();

  static void run(WriteThread* self);

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

  TimePoint _last_warn_time;
  TimePoint _last_stats_time;
  TimePoint _last_input_time;

  // Recorded stats
  std::atomic<size_t> _average_buf_size;
  std::atomic<size_t> _max_buf_size;

  /** Rolling average of last 100 buffers */
  std::atomic<double> _input_average_bps;
  std::queue<double> _input_bps_queue;

  /** Rolling average of last 100 buffers */
  std::atomic<double> _output_average_bps;
  std::queue<double> _output_bps_queue;
};

#endif // SWIFTNAV_WRITE_THREAD_H
