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

#include "write_thread.h"
#include "rotating_logger.h"

WriteThread::WriteThread(
#ifndef TEST_WRITE_THREAD
  RotatingLogger& logger
#else
  write_thread_func_t func
#endif
)
    : _read_index(0),
      _write_index(0),
      _stop(false),
#ifndef TEST_WRITE_THREAD
      _logger(logger)
#else
      _write_func(func)
#endif
{
}

void WriteThread::start() {

  try {
    _thread = std::thread(run, this);
  } catch(std::exception exc) {
#ifdef TEST_WRITE_THREAD
    printf("Failed to start...\n");
#endif
  }

  std::unique_lock<std::mutex> lock(_thread_mutex);
  _event.wait(lock);
}

uint16_t WriteThread::queue_depth() {
  return _write_index == _read_index ? 0 : 1;
}

void WriteThread::join() {
  try {
    _thread.join();
  } catch (const std::exception& exc){
    return;
  }
}

void WriteThread::stop() {
  _stop = true;

//  std::unique_lock<std::mutex> lock(_thread_mutex);
//  _event.notify_one();

  if (_thread_mutex.try_lock())
    _event.notify_one();
}

#ifndef TEST_WRITE_THREAD
#  define WARN(M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  _logger.log_msg(LOG_WARNING, _Msg1); \ }
#else
#  define WARN(M, ...) { printf(M "\n", ##__VA_ARGS__); }
#endif

#ifndef TEST_WRITE_THREAD
#  define DEBUG(M, ...)
#else
#  define DEBUG(M, ...) { printf(M "\n", ##__VA_ARGS__); }
#endif

void WriteThread::queue_data(const uint8_t* data, size_t size) {

  {
    std::lock_guard<std::mutex> lock(_thread_mutex);

    uint8_t wi = _write_index;
    uint8_t ri = _read_index;

    if ( static_cast<uint8_t>(ri - (wi + 1)) == 0U ) {

      WARN("Dropping data, write queue full");
      _event.notify_one();

      return;
    }

    write_args args{data, size};

    std::swap(_queue[_write_index++], args);
    _event.notify_one();
  }

  DEBUG("Finished queueing data (%d, %d)...", _read_index.load(), _write_index.load());
}

void WriteThread::run(WriteThread* self)
{
    DEBUG("Starting thread... \n");
    {
      std::unique_lock<std::mutex> lock(self->_thread_mutex);
      self->_event.notify_one();
    }

#ifdef TEST_WRITE_THREAD
# define DEBUG_QUIT() printf("Quitting...\n");
#else
# define DEBUG_QUIT()
#endif

#define HANDLE_QUIT() \
    if (self->_stop) { DEBUG_QUIT(); return; }
      
    for (;;) {

      HANDLE_QUIT()
      std::unique_lock<std::mutex> lock(self->_thread_mutex);

      DEBUG("Waiting for data...\n");

      self->_event.wait(lock);
      lock.unlock();

      HANDLE_QUIT()

      DEBUG("Handling posted write data (%d, %d)...\n",
        self->_read_index.load(), self->_write_index.load());

      while (self->_read_index != self->_write_index) {

        write_args args;
        std::swap(args, self->_queue[self->_read_index++]);

#ifndef TEST_WRITE_THREAD
        self->_logger._frame_handler(args.data, args.size);
#else
        printf("Data %p %zu, func ptr %p\n", args.data, args.size, self->_write_func);
        self->_write_func(args.data, args.size);
#endif
        HANDLE_QUIT();
      }
    }
}

#undef HANDLE_QUIT
#undef DEBUG_QUIT
