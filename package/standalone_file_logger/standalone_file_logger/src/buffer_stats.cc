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

#include "buffer_stats.h"
#include "debug_macros.h"

using namespace std::chrono;

static const int STATS_ISSUE_TIMEOUT_MSECS = 5000;

BufferStats::BufferStats(LogCall log_msg)
    : _average_buf_size(0),
      _max_buf_size(0),
      _input_average_bps(0),
      _output_average_bps(0),
      _log_msg(log_msg)
{
}

void BufferStats::handle_stats_report(size_t queue_depth, size_t alloc_bytes) {

  if ( msecs_since_last_stats() >= STATS_ISSUE_TIMEOUT_MSECS ) {

  	update_input_bps();

    _WT_DEBUG(this,
             "Buffer input/output stats: max buf: %zu, avg buf: %zu, input bps: %.02f"
               " output bps: %.02f, queue depth: %zu, alloced bytes: %zu",
             (size_t)_max_buf_size,
             (size_t)_average_buf_size,
             (double)_input_average_bps,
             (double)_output_average_bps,
             queue_depth,
             alloc_bytes);

     _last_stats_time = steady_clock::now(); 
  }
}

size_t BufferStats::msecs_since_last_stats()
{
  return duration_cast<milliseconds>(steady_clock::now() - _last_stats_time).count();
}

void BufferStats::update_buffer_stats(size_t size, size_t buffer_count)
{
  size_t average = _average_buf_size;

  average = (size + (buffer_count*average)) / (buffer_count+1);
  _average_buf_size = average;

  _max_buf_size = std::max(size, (size_t)_max_buf_size);
}

void BufferStats::update_output_bps(size_t size, TimePoint& write_start_time)
{
  double output_average_bps = _output_average_bps; // load atomic

  update_bps_impl(size,
                  write_start_time,
                  _output_bps_queue,
                  output_average_bps,
                  /* update_last_time = */false);

  _output_average_bps = output_average_bps; // update atomic
}

void BufferStats::update_input_bps()
{
  double input_average_bps = _input_average_bps; // load atomic

  update_bps_impl(_input_bps_accum,
                  _last_input_time,
                  _input_bps_queue,
                  input_average_bps,
                  /* update_last_time = */true);

  _input_average_bps = input_average_bps; // update atomic
	_input_bps_accum = 0;
}

void BufferStats::update_bps_impl(size_t                size,
                                  TimePoint&            last_time,
                                  std::queue<double>&   bps_value_queue,
                                  double&               average_bps,
                                  bool                  update_last_time/* = false*/)
{
  const size_t MAX_ITEM_COUNT = 100;

  bool is_unset = duration_cast<milliseconds>(
                    last_time.time_since_epoch()).count() == 0;

  if (is_unset) {
    last_time = steady_clock::now();
    return;
  }

  double micros_since_last =
    duration_cast<microseconds>(steady_clock::now() - last_time).count();

  double bps = 1E6 * (size / micros_since_last);

  size_t item_count = bps_value_queue.size();

  if ( item_count < MAX_ITEM_COUNT ) {

    // Update as if cumulative moving average
    average_bps = (bps + (item_count*average_bps)) / (item_count+1);
    bps_value_queue.push(bps);

  } else {

    double first_bps = bps_value_queue.front();

    bps_value_queue.pop();
    bps_value_queue.push(bps);

    average_bps = (average_bps + (bps / MAX_ITEM_COUNT) - (first_bps / MAX_ITEM_COUNT));
  }

  if (update_last_time)
    last_time = steady_clock::now();
}
