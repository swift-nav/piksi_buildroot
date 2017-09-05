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
  S->_callbacks.log_msg(LOG_WARNING, _Msg1); }

#ifndef TEST_WRITE_THREAD
#  define _WT_DEBUG(M, ...)
#else
#  define _WT_DEBUG(S, M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  S->_callbacks.log_msg(LOG_DEBUG, _Msg1); }
#endif

WriteThread::WriteThread(const Callbacks& callbacks)
    : _read_index(0),
      _write_index(0),
      _stop(false),
      _callbacks(callbacks)
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

uint16_t WriteThread::queue_depth() {
  return _write_index == _read_index ? 0 : 1;
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
  _stop = true;
  if (_thread_mutex.try_lock())
    _event.notify_one();
}
void WriteThread::queue_data(const uint8_t* data, size_t size) {

  {
    std::lock_guard<std::mutex> lock(_thread_mutex);

    uint8_t wi = _write_index;
    uint8_t ri = _read_index;

    if ( static_cast<uint8_t>(ri - (wi + 1)) == 0U ) {

      _WT_WARN(this, "Dropping data, write queue full");
      _event.notify_one();

      return;
    }

    write_args args{data, size};

    std::swap(_queue[_write_index++], args);
    _event.notify_one();
  }

  _WT_DEBUG(this, "Finished queueing data (%d, %d)...", _read_index.load(), _write_index.load());
}

void WriteThread::run(WriteThread* self)
{
  static auto WAIT_TIMEOUT = std::chrono::milliseconds(100);
  std::unique_lock<std::mutex> start_lock(self->_start_mutex);

  _WT_DEBUG(self, "Starting thread... \n");
  start_lock.unlock();
  self->_start_event.notify_one();

# ifdef TEST_WRITE_THREAD
#   define _WT_DEBUG_QUIT() _WT_DEBUG(self, "Quitting...\n");
# else
#   define _WT_DEBUG_QUIT()
# endif

# define _WT_HANDLE_QUIT() if (self->_stop) { _WT_DEBUG_QUIT(); return; }
      
  for (;;) {

    _WT_HANDLE_QUIT()

    std::unique_lock<std::mutex> lock(self->_thread_mutex);

    _WT_DEBUG(self, "Waiting for data...\n");

    self->_event.wait_for(lock, WAIT_TIMEOUT);
    lock.unlock();

    _WT_HANDLE_QUIT()

    _WT_DEBUG(self, "Handling posted write data (%d, %d)...\n",
      self->_read_index.load(), self->_write_index.load());

    while (self->_read_index != self->_write_index) {

      write_args args;
      std::swap(args, self->_queue[self->_read_index++]);

      self->_callbacks.handle_write(args.data, args.size);

      _WT_HANDLE_QUIT();
    }
  }

# undef _WT_HANDLE_QUIT
# undef _WT_DEBUG_QUIT
}

