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

#include <sys/syslog.h>
#include <chrono>

#include <pthread.h>
#include <sched.h>

#include "write_thread.h"
#include "rotating_logger.h"

using namespace std::chrono;

static const int MAX_ALLOC = 2 * 1024 * 1024; // 2 megabytes

static const int OVERFLOW_WARN_TIMEOUT_SECS = 1;
static const int STATS_ISSUE_TIMEOUT_MSECS = 5000;

#  define _WT_WARN(S, M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  if (S->_log_msg) S->_log_msg(LOG_WARNING, _Msg1); }

#  define _WT_DEBUG(S, M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  if (S->_log_msg) S->_log_msg(LOG_DEBUG, _Msg1); }

#ifndef TEST_WRITE_THREAD
#  define _WT_DEBUG_TEST(M, ...)
#else
#  define _WT_DEBUG_TEST(S, M, ...) _WT_DEBUG(S, M, ##__VA_ARGS__)
#endif

//#  define _WT_WARN(S, M, ...) printf(M "\n", ##__VA_ARGS__)
//#  define _WT_DEBUG_TEST(S, M, ...) printf(M "\n", ##__VA_ARGS__)

WriteThread::WriteThread()
    : _alloc_bytes(0),
      _average_buf_size(0),
      _max_buf_size(0),
      _input_average_bps(0),
      _output_average_bps(0)
{
}

WriteThread::~WriteThread()
{
  free_queue();
}

void WriteThread::start()
{
  std::unique_lock<std::mutex> lock(_start_mutex);

  try {
    _thread = std::thread(run, this);
#ifdef __linux__
    sched_param params{0};
    pthread_setschedparam(_thread.native_handle(), SCHED_IDLE, &params);
#endif

  } catch(std::exception exc) {
    _WT_DEBUG_TEST(this, "Failed to start...\n");
  }

  _start_event.wait(lock);
}

bool WriteThread::queue_empty()
{
  std::lock_guard<std::mutex> lock(_thread_mutex);

  _WT_DEBUG_TEST(this, "Queue depth: %zu", _queue.size());
  return _queue.empty();
}

size_t WriteThread::alloc_bytes()
{
  return _alloc_bytes;
}

void WriteThread::set_callbacks(const LogCall& log_call, const WriteCall& handle_write) {
  
  std::lock_guard<std::mutex> lock(_thread_mutex);

  _log_msg = log_call;
  _handle_write = handle_write;
}

void WriteThread::join() {
  if (!_thread.joinable()) {
    return;
  }
  try {
    _thread.join();
  } catch (const std::exception& exc){
    return;
  }
}

void WriteThread::stop() {

  _WT_DEBUG_TEST(this, "Trying to stop...");
  queue_data_impl(nullptr, 0, true);
}

void WriteThread::queue_data(const uint8_t* data, size_t size) {
  queue_data_impl(data, size, false);
}

size_t WriteThread::msecs_since_last_stats()
{
  return duration_cast<milliseconds>(steady_clock::now() - _last_stats_time).count();
}

size_t WriteThread::seconds_since_last_warn()
{
  return duration_cast<seconds>(steady_clock::now() - _last_warn_time).count();
}

void WriteThread::update_buffer_stats(size_t size)
{
  size_t average = _average_buf_size;
  size_t buffer_count = _queue.size();

  average = (size + (buffer_count*average)) / (buffer_count+1);
  _average_buf_size = average;

  _max_buf_size = std::max(size, (size_t)_max_buf_size);
}

void WriteThread::update_output_bps(size_t size, TimePoint& write_start_time)
{
  double output_average_bps = _output_average_bps; // load atomic

  update_bps_impl(size,
                  write_start_time,
                  _output_bps_queue,
                  output_average_bps,
                  /* update_last_time = */false);

  _output_average_bps = output_average_bps; // update atomic
}

void WriteThread::update_input_bps(size_t size)
{
  double input_average_bps = _input_average_bps; // load atomic

  update_bps_impl(size,
                  _last_input_time,
                  _input_bps_queue,
                  input_average_bps,
                  /* update_last_time = */true);

  _input_average_bps = input_average_bps; // update atomic
}

void WriteThread::update_bps_impl(size_t                size,
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

void WriteThread::queue_data_impl(const uint8_t* data, size_t size, bool stop) {

  if (stop) {

    std::lock_guard<std::mutex> lock(_thread_mutex);
    _queue.push_back(write_args{stop, nullptr, 0});

    return;
  }

  if ( _alloc_bytes > MAX_ALLOC ) {

    if ( seconds_since_last_warn() >= OVERFLOW_WARN_TIMEOUT_SECS ) {

        _WT_WARN(this,
                 "Dropping data, allocated data over maximum: %zu bytes, "
                 "queue depth: %zu elements.",
                 (size_t)_alloc_bytes, _queue.size());

        _last_warn_time = std::chrono::steady_clock::now();
    }

    std::lock_guard<std::mutex> lock(_new_data_mutex);
    _new_data_event.notify_one();

    return;
  }

  update_buffer_stats(size);
  update_input_bps(size);

  if ( msecs_since_last_stats() >= STATS_ISSUE_TIMEOUT_MSECS ) {

    _WT_DEBUG(this,
             "Buffer input/output stats: max buf: %zu, avg buf: %zu, input bps: %.02f"
               " output bps: %.02f, queue depth: %zu, alloced bytes: %zu",
             (size_t)_max_buf_size,
             (size_t)_average_buf_size,
             (double)_input_average_bps,
             (double)_output_average_bps,
             _queue.size(),
             (size_t)_alloc_bytes);

     _last_stats_time = steady_clock::now(); 
  }

  uint8_t* data_copy = new uint8_t[size];
  std::copy(data, data + size, data_copy);
  _alloc_bytes += size;

  {
    std::lock_guard<std::mutex> lock(_thread_mutex);
    _queue.push_back(write_args{stop, data_copy, size});
  }
  {
    std::lock_guard<std::mutex> lock(_new_data_mutex);
    _new_data_event.notify_one();
  }

  _WT_DEBUG_TEST(this, "Finished queueing data...");
}

void WriteThread::run(WriteThread* self)
{
  static auto WAIT_TIMEOUT = std::chrono::milliseconds(30);
  std::unique_lock<std::mutex> start_lock(self->_start_mutex);

  _WT_DEBUG_TEST(self, "Starting thread... ");
  start_lock.unlock();
  self->_start_event.notify_one();

  for (;;) {

    _WT_DEBUG_TEST(self, "Waiting for data...");

    std::unique_lock<std::mutex> lock(self->_new_data_mutex);
    self->_new_data_event.wait_for(lock, WAIT_TIMEOUT);
    lock.unlock();

    _WT_DEBUG_TEST(self, "Handling posted write data...");

    for (;;) {

      write_args args;
      {
        std::lock_guard<std::mutex> lock(self->_thread_mutex);

        if (self->_queue.empty())
          break;

        args = self->_queue.front();
        self->_queue.pop_front();
      }

      if (args.stop) {
        _WT_DEBUG_TEST(self, "Quitting...");
        return;
      }

      if(!self->_handle_write)
        _WT_WARN(self, "No callback setup for handle_write, crash imminent...");

      TimePoint write_start = steady_clock::now();
      self->_handle_write(args.data, args.size);

      self->update_output_bps(args.size, write_start);

      self->_alloc_bytes -= args.size;
      delete [] args.data;
    }
  }
}

void WriteThread::free_queue()
{
  std::lock_guard<std::mutex> lock(_thread_mutex);

  while (!_queue.empty()) {

    auto args = _queue.front();
    if (!args.stop)
      delete [] args.data;

    _queue.pop_front();
  }
}
