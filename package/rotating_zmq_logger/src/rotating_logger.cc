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

#include "rotating_logger.h"

#include <assert.h>
#include <dirent.h>
#include <stdarg.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Name format: xxxx-yyyyy.sbp
 * First four digit number (xxxx) is the session counter.
 * This increments each time the process doing the logging is restarted.
 * The number to start from comes from parsing the files that exist on the SD
 * card.
 * It starts at 1 rather than 0.
 * The logger asserts if a session 9999 file already exists.
 *
 * The second 5 digit number (yyyyy) is the number of minutes since the linux
 * process
 * doing the logging started.  It represents the BEGINNING of the logfile.
 * If minutes exceeds 99999 the session is incremented.
 */
static const std::string LOG_SUFFIX = ".sbp";
static const size_t LOG_NAME_LEN = 4 + 1 + 5 + 4;

void RotatingLogger::debug_printf(const char* msg, ...) {
  if (!_verbose_logging) {
    return;
  }

  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
}

int RotatingLogger::check_disk_full() {
  struct statvfs fs_stats;
  if (statvfs(_out_dir.c_str(), &fs_stats)) {
    return -1;
  }
  double percent_full = double(fs_stats.f_blocks - fs_stats.f_bavail) /
                        double(fs_stats.f_blocks) * 100.;
  if (percent_full < _disk_full_threshold) {
    return 0;
  }
  return 1;
}

bool RotatingLogger::open_new_file() {
  if (_session_count >= 9999) {
    perror("Max session exceeded");
    return false;
  }
  if (_cur_file != -1) {
    close(_cur_file);
    _cur_file = -1;
  }

  int fs_status = check_disk_full();
  if (fs_status == 1) {
    debug_printf("Target dir full\n");
    return false;
  }

  char log_name_buf[LOG_NAME_LEN + 1];
  if (_minute_count > 99999) {
    debug_printf("Minutes roll over\n");
    _minute_count = 0;
    _session_count++;
  }
  sprintf(log_name_buf, "%04lu-%05lu%s", _session_count, _minute_count,
          LOG_SUFFIX.c_str());
  // Need to use open instead of fopen to avoid locking drive
  _cur_file = open((_out_dir + "/" + log_name_buf).c_str(), O_WRONLY | O_CREAT, 0666);
  _dest_available = _cur_file != -1;
  debug_printf("Opening file %s: %d\n", log_name_buf, _dest_available);
  if (!_dest_available) {
    perror("Error openning file");
  }
  return _dest_available;
}

bool RotatingLogger::check_slice_time() {
  double diff_minutes = get_time_passed() / 60. - _minute_count;
  size_t periods = diff_minutes / _slice_duration;
  if (periods <= 0) {
    return true;
  }
  debug_printf("Rolling over log\n");
  // if multiple roll overs occured skip files with no data
  _minute_count += _slice_duration * periods;
  return open_new_file();
}

bool RotatingLogger::start_new_session() {
  DIR* dirp = opendir(_out_dir.c_str());
  struct dirent* dp = nullptr;
  if (dirp == nullptr) {
    debug_printf("Target dir unavailable\n");
    return false;
  }
  // check files in path for last session index
  // pick up where they left off
  _session_count = 0;
  _minute_count = 0;
  _session_start_time = std::chrono::steady_clock::now();
  while ((dp = readdir(dirp)) != nullptr) {
    if (strlen(dp->d_name) == LOG_NAME_LEN &&
        std::string(dp->d_name).find(LOG_SUFFIX) != std::string::npos) {
      size_t file_count = strtol(dp->d_name, nullptr, 10);
      if (file_count < 9999 && file_count > _session_count) {
        _session_count = file_count;
      }
    }
  }
  closedir(dirp);
  _session_count++;
  return open_new_file();
}

double RotatingLogger::get_time_passed() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now() - _session_start_time)
      .count();
}

void RotatingLogger::frame_handler(const uint8_t* data, size_t size) {
  if (!_dest_available) {
    // check imediately on startup for path availability. Subsequently, check
    // periodically
    if (_session_start_time.time_since_epoch().count() != 0 &&
        get_time_passed() < _poll_period) {
      return;
    }
    _session_start_time = std::chrono::steady_clock::now();
    if (!start_new_session()) {
      return;
    }
  }
  if (!check_slice_time()) {
    return;
  }

  size_t num_written = 0;
  if (_cur_file != -1) {
    num_written = write(_cur_file, data, size);
  }
  if (num_written != size) {
    // If drive is removed needs to close file imediately and not attempt to
    // open new file for a couple seconds to avoid locking mount
    close(_cur_file);
    _cur_file = -1;
    _dest_available = false;
    // wait _poll_period to check drive again
    _session_start_time = std::chrono::steady_clock::now();
    perror("Write to file failed");
  }
}

RotatingLogger::RotatingLogger(const std::string& out_dir,
                               size_t slice_duration, size_t poll_period,
                               size_t disk_full_threshold,
                               bool verbose_logging)
    : _dest_available(false),
      _session_count(0),
      _minute_count(0),
      _out_dir(out_dir),
      _slice_duration(slice_duration),
      _poll_period(poll_period),
      _disk_full_threshold(disk_full_threshold),
      _verbose_logging(verbose_logging),
      // init to 0
      _session_start_time(),
      _cur_file(-1) {}

RotatingLogger::~RotatingLogger() {
  if (_cur_file != -1) {
    close(_cur_file);
  }
}
