/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jonathan Diamond <jonathan@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_ROTATING_LOGGER_H
#define SWIFTNAV_ROTATING_LOGGER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <string>
#include <functional>


class RotatingLogger {

  /* Pad new files out to minimize filesystem updates */
  static const size_t NEW_FILE_PAD_SIZE = 15*1024*1024;

 public:
  typedef std::function<void(int, const char *)> LogCall;
  RotatingLogger(const std::string& out_dir, size_t slice_duration,
                 size_t poll_period, size_t disk_full_threshold,
                 LogCall logging_callback = LogCall());

  ~RotatingLogger();
  /*
   * try to log a data frame
   */
  void frame_handler(const uint8_t* data, size_t size);

  /*
   * Update output directory. Subsequent files will use this path.
   */
  void update_dir(const std::string& out_dir);

  /*
   * Update fill threshold. Subsequent files will check this threshold.
   */
  void update_fill_threshold(size_t disk_full_threshold);

  /*
   * Update slice duration. This will apply to current log.
   */
  void update_slice_duration(size_t slice_duration);

 protected:
  /*
   * Try to start a new session. Return true on success
   */
  bool start_new_session();
  /*
   * Try to create a new log. Return true on success
   */
  bool open_new_file();
  /*
   * See if slice_duration is exceeded and log needs to roll. Return true on
   * rollover
   */
  bool check_slice_time();
  /*
   * Get time passed from _session_start_time
   */
  double get_time_passed();
  /*
   * Check if the percent disk unavailable is below the threshold. 0 on below, 1
   * on above, -1 on error
   */
  int check_disk_full();
  /*
   * print if _verbose_logging
   */
  void log_msg(int priority, const std::string &msg);
  /* 
   * Flush and close the current file
   */
  void close_current_file();
  /*
   * Pad out current file
   */
  void pad_new_file();

  void log_errno_warning(const char* msg);

  bool _dest_available;
  size_t _session_count;
  size_t _minute_count;
  size_t _slice_duration;
  size_t _poll_period;
  size_t _disk_full_threshold;
  LogCall _logging_callback;
  std::string _out_dir;
  std::chrono::time_point<std::chrono::steady_clock> _session_start_time;

  FILE* _cur_file;
  size_t _bytes_written;
};

#endif  // SWIFTNAV_ROTATING_LOGGER_H
