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

#  define _WT_WARN(S, M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  if (S->_log_msg) S->_log_msg(LOG_WARNING, _Msg1); }

#ifndef TEST_WRITE_THREAD
#  define _WT_DEBUG(M, ...)
#else
#  define _WT_DEBUG(S, M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  if (S->_log_msg) S->_log_msg(LOG_DEBUG, _Msg1); }
#endif

//#  define _WT_WARN(S, M, ...) printf(M "\n", ##__VA_ARGS__)
//#  define _WT_DEBUG(S, M, ...) printf(M "\n", ##__VA_ARGS__)

WriteThread::WriteThread()
    : _alloc_bytes(0)
{
}

void WriteThread::start() {

  std::unique_lock<std::mutex> lock(_start_mutex);

  try {
    _thread = std::thread(run, this);
#ifdef __linux__
    sched_param params{0};
    pthread_setschedparam(_thread.native_handle(), SCHED_IDLE, &params);
#endif

  } catch(std::exception exc) {
    _WT_DEBUG(this, "Failed to start...\n");
  }

  _start_event.wait(lock);
}

bool WriteThread::queue_empty() {

  std::lock_guard<std::mutex> lock(_thread_mutex);

  _WT_DEBUG(this, "Queue depth: %zu", _queue.size());
  return _queue.empty();
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

  _WT_DEBUG(this, "Trying to stop...");
  _queue_data(nullptr, 0, true);
}

void WriteThread::queue_data(const uint8_t* data, size_t size) {
  _queue_data(data, size, false);
}

void WriteThread::_queue_data(const uint8_t* data, size_t size, bool stop) {

  if (stop) {

    std::lock_guard<std::mutex> lock(_thread_mutex);
    _queue.push_front(write_args{stop, nullptr, 0});

    return;
  }

  if ( _alloc_bytes > MAX_ALLOC ) {

    _WT_WARN(this,
             "Dropping data, allocated data over maximum: %zu bytes, "
             "queue depth: %zu elements.",
             (size_t)_alloc_bytes, _queue.size());

    std::lock_guard<std::mutex> lock(_new_data_mutex);
    _new_data_event.notify_one();

    return;
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

  _WT_DEBUG(this, "Finished queueing data...");
}

void WriteThread::run(WriteThread* self)
{
  static auto WAIT_TIMEOUT = std::chrono::milliseconds(30);
  std::unique_lock<std::mutex> start_lock(self->_start_mutex);

  _WT_DEBUG(self, "Starting thread... ");
  start_lock.unlock();
  self->_start_event.notify_one();

  for (;;) {

    _WT_DEBUG(self, "Waiting for data...");

    std::unique_lock<std::mutex> lock(self->_new_data_mutex);
    self->_new_data_event.wait_for(lock, WAIT_TIMEOUT);
    lock.unlock();

    _WT_DEBUG(self, "Handling posted write data...");

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
        _WT_DEBUG(self, "Quitting...");
        return;
      }

      if(!self->_handle_write)
        _WT_WARN(self, "No callback setup for handle_write, crash imminent...");

      self->_handle_write(args.data, args.size);
      self->_alloc_bytes -= args.size;

      delete [] args.data;
    }
  }
}
