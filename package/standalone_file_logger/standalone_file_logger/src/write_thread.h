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

#include "buffer_stats.h"

class RotatingLogger;

class WriteThread
{
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

  WriteThread(WriteThread const&) = delete;

public:
  static const size_t BLOCK_SIZE = 4096;

  typedef std::function<void(const uint8_t* data, size_t size)> WriteCall;
  typedef std::function<void(int, const char *)> LogCall;

  WriteThread();
  ~WriteThread();

  void start();
  void join();
  void stop();

  bool queue_empty();
  size_t alloc_bytes();

  void queue_data(const uint8_t* data, size_t size); 
  void set_callbacks(const LogCall& log_call, const WriteCall& handle_write);

private:
  void queue_data_impl(const uint8_t* data, size_t size, bool stop); 
  void flush_current_block();
  void free_queue();
  size_t seconds_since_last_warn();

  static void run(WriteThread* self);

	class Event {

	public:
		inline std::mutex& mutex()
			{ return _mutex; }

		inline void wait(std::unique_lock<std::mutex>& lock)
			{ _event.wait(lock); }

		inline void lock_and_wait(std::chrono::milliseconds max_wait, bool unlock = true) {

    	std::unique_lock<std::mutex> lock(_mutex);
 			_event.wait_for(lock, max_wait);
			if (unlock) lock.unlock();
		}

		inline void signal() {
			std::lock_guard<std::mutex> lock(_mutex);
			_event.notify_one();
		}

	private:
		std::condition_variable _event;
		std::mutex _mutex;
	};

  uint8_t* _block_buffer;
  uint8_t* _block_buffer_head;

  size_t _block_remaining;

  struct write_args {
    bool stop;
    const uint8_t* data;
    size_t size;
  };

  LogCall _log_msg;
  WriteCall _handle_write;

	Event _start_event;
	Event _data_event;

  std::mutex _thread_mutex;

  std::deque<write_args> _queue;
  std::atomic<size_t> _alloc_bytes;

  std::thread _thread;

	BufferStats _buffer_stats;

  TimePoint _last_warn_time;
};

#endif // SWIFTNAV_WRITE_THREAD_H
