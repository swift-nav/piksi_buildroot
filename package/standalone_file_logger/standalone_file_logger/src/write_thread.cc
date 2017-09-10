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

#include <pthread.h>
#include <sched.h>

#include "write_thread.h"
#include "rotating_logger.h"

#include "debug_macros.h"

using namespace std::chrono;

static const int MAX_ALLOC = 2 * 1024 * 1024; // 2 megabytes
static const int OVERFLOW_WARN_TIMEOUT_SECS = 1;
static const auto WAIT_THREAD_MAX_SLEEP = std::chrono::milliseconds(30);

WriteThread::WriteThread()
    : _alloc_bytes(BLOCK_SIZE),
      _block_buffer_head(new uint8_t[BLOCK_SIZE]),
      _block_remaining(BLOCK_SIZE),
		  _buffer_stats(_log_msg)
{
  _block_buffer = _block_buffer_head;
}

WriteThread::~WriteThread()
{
  free_queue();
  delete[] _block_buffer_head;
}

void WriteThread::start()
{
  std::unique_lock<std::mutex> lock(_start_event.mutex());

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

  flush_current_block();
  queue_data_impl(nullptr, 0, true);
}

void WriteThread::queue_data(const uint8_t* data, size_t size) {
  queue_data_impl(data, size, false);
}

size_t WriteThread::seconds_since_last_warn()
{
  return duration_cast<seconds>(steady_clock::now() - _last_warn_time).count();
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

		_data_event.signal();

    return;
  }

  size_t buffer_count = _queue.size();
  _buffer_stats.update_buffer_stats(size, buffer_count);
	_buffer_stats.accum_input_bps(size);

	_buffer_stats.handle_stats_report(_queue.size(), _alloc_bytes);

  size_t bytes_remaining = size;

  while (bytes_remaining > 0) {

    if ( bytes_remaining < _block_remaining ) {

      std::copy(data, data + bytes_remaining, _block_buffer);

      // Consume block buffer
      _block_remaining -= bytes_remaining;
      _block_buffer += bytes_remaining;

      // Mark all bytes queued
      bytes_remaining = 0;

    } else {

      std::copy(data, data + _block_remaining, _block_buffer);

      bytes_remaining -= _block_remaining;
      data += _block_remaining;
      _block_remaining = 0;

      flush_current_block();
    }
  }

  _WT_DEBUG_TEST(this, "Finished queueing data...");
}

void WriteThread::flush_current_block()
{
	std::lock_guard<std::mutex> lock(_thread_mutex);

	size_t flushed_buf_size = BLOCK_SIZE - _block_remaining;
	if ( flushed_buf_size == 0 ) {
		return;
	}

	_queue.push_back(write_args{/*stop=*/false, _block_buffer_head, flushed_buf_size});

	_alloc_bytes += BLOCK_SIZE;

	_block_buffer_head = new uint8_t[BLOCK_SIZE];
	_block_buffer = _block_buffer_head;

	_block_remaining = BLOCK_SIZE;

	_data_event.signal();
}

void WriteThread::run(WriteThread* self)
{
  _WT_DEBUG_TEST(self, "Starting thread... ");
	self->_start_event.signal();

  for (;;) {

    _WT_DEBUG_TEST(self, "Waiting for data...");
		self->_data_event.lock_and_wait(WAIT_THREAD_MAX_SLEEP);

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
      self->_buffer_stats.update_output_bps(args.size, write_start);

      self->_alloc_bytes -= BLOCK_SIZE;
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
