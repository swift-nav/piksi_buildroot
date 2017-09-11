/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jason Mobarak <jason@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_BUFFER_STATS_H
#define SWIFTNAV_BUFFER_STATS_H

#include <atomic>
#include <chrono>
#include <queue>
#include <functional>

class BufferStats {

  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
  typedef std::function<void(int, const char *)> LogCall;

public:
  BufferStats(LogCall log_msg);

  void handle_stats_report(size_t queue_depth, size_t alloc_bytes);

  void update_buffer_stats(size_t size, size_t buffer_count);

  inline void accum_input_bps(size_t size) { _input_bps_accum += size; }

  void update_input_bps();
  void update_output_bps(size_t size, TimePoint& write_start_time);

private:
  size_t msecs_since_last_stats();

  static void update_bps_impl(size_t size,
                              TimePoint& last_time,
                              std::queue<double>& bps_value_queue,
                              double& average_bps,
                              bool update_last_time = false);

  TimePoint _last_stats_time;
  TimePoint _last_input_time;

  //// Recorded stats on buffers ////

  std::atomic<size_t> _average_buf_size;
  std::atomic<size_t> _max_buf_size;

  /** Rolling average of last N buffers */
  std::atomic<size_t> _input_bps_accum;
  std::atomic<double> _input_average_bps;
  std::queue<double> _input_bps_queue;

  /** Rolling average of last N buffers */
  std::atomic<double> _output_average_bps;
  std::queue<double> _output_bps_queue;

  LogCall _log_msg;
};

#endif//SWIFTNAV_BUFFER_STATS_H
