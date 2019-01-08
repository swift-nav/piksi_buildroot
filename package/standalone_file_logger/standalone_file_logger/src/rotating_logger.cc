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
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syslog.h>
#include <libpiksi/logging.h>

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

void RotatingLogger::log_msg(int priority, const std::string &msg)
{
  piksi_log(LOG_INFO, "log_msg");
  if (_logging_callback) {
    _logging_callback(priority, msg.c_str());
  }
}

void RotatingLogger::close_current_file()
{
  piksi_log(LOG_INFO, "close_current_file");
  if (_cur_file != nullptr) {

    piksi_log(LOG_INFO, "close_current_file not null");
    if (_bytes_written < NEW_FILE_PAD_SIZE) {
      ftruncate(fileno(_cur_file), _bytes_written);
    }

    piksi_log(LOG_INFO, "close_current_file fflush");
    fflush(_cur_file);
    piksi_log(LOG_INFO, "close_current_file fsync");
    fsync(fileno(_cur_file));
    piksi_log(LOG_INFO, "close_current_file fclose");
    fclose(_cur_file);

    _cur_file = nullptr;
    _bytes_written = 0;
  }
  piksi_log(LOG_INFO, "close_current_file fin");
}

void RotatingLogger::log_errno_warning(const char *msg)
{
  piksi_log(LOG_INFO, "log_errno_warning");
  log_msg(LOG_WARNING, std::string(msg) + ":" + strerror(errno));
}

void RotatingLogger::pad_new_file()
{
  piksi_log(LOG_INFO, "pad_new_file");

  if (_cur_file != nullptr) {
    piksi_log(LOG_INFO, "padding file");

    if (ftruncate(fileno(_cur_file), NEW_FILE_PAD_SIZE) != 0)
      log_errno_warning("Error while padding new file");

    if (fsync(fileno(_cur_file)) != 0) log_errno_warning("Error while syncing newly padded file");
  }
}

int RotatingLogger::check_disk_full()
{
  piksi_log(LOG_INFO, "check_disk_full");
  struct statvfs fs_stats;
  if (statvfs(_out_dir.c_str(), &fs_stats)) {
    return -1;
  }
  double percent_full =
    double(fs_stats.f_blocks - fs_stats.f_bavail) / double(fs_stats.f_blocks) * 100.;
  if (percent_full < _disk_full_threshold) {
    return 0;
  }
  return 1;
}

bool RotatingLogger::open_new_file()
{
  piksi_log(LOG_INFO, "open_new_file");
  if (_session_count >= 9999) {
    perror("Max session exceeded");
    return false;
  }
  close_current_file();
  int fs_status = check_disk_full();
  if (fs_status == 1) {
    log_msg(LOG_WARNING, std::string("Target dir full"));
    return false;
  }

  char log_name_buf[LOG_NAME_LEN + 1];
  if (_minute_count > 99999) {
    log_msg(LOG_WARNING, std::string("Minutes roll over"));
    _minute_count = 0;
    _session_count++;
  }
  sprintf(log_name_buf, "%04lu-%05lu%s", _session_count, _minute_count, LOG_SUFFIX.c_str());

  _cur_file = fopen((_out_dir + "/" + log_name_buf).c_str(), "wb");

  if (_cur_file != nullptr && ferror(_cur_file) != 0) {
    fclose(_cur_file);
    _cur_file = nullptr;
  }

  _dest_available = _cur_file != nullptr;

  if (!_dest_available) {
    log_msg(LOG_WARNING, std::string("Error openning file: ") + strerror(errno));
  }

  pad_new_file();

  if (_dest_available) {
    log_msg(LOG_INFO, std::string("Opened file: ") + log_name_buf);
  }

  return _dest_available;
}

bool RotatingLogger::check_slice_time()
{
  double diff_minutes = get_time_passed() / 60. - _minute_count;
  size_t periods = diff_minutes / _slice_duration;
  if (periods <= 0) {
    return true;
  }
  log_msg(LOG_INFO, "Rolling over log");
  // if multiple roll overs occured skip files with no data
  _minute_count += _slice_duration * periods;
  return open_new_file();
}

bool RotatingLogger::start_new_session()
{
  piksi_log(LOG_INFO, "start_new_session");
  DIR *dirp = opendir(_out_dir.c_str());
  struct dirent *dp = nullptr;
  if (dirp == nullptr) {
    log_msg(LOG_WARNING, "Target dir unavailable");
    return false;
  }
  // check files in path for last session index
  // pick up where they left off
  _session_count = 0;
  _minute_count = 0;
  _session_start_time = std::chrono::steady_clock::now();
  while ((dp = readdir(dirp)) != nullptr) {
    if (strlen(dp->d_name) == LOG_NAME_LEN
        && std::string(dp->d_name).find(LOG_SUFFIX) != std::string::npos) {
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

double RotatingLogger::get_time_passed()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()
                                                          - _session_start_time)
    .count();
}

void RotatingLogger::frame_handler(const uint8_t *data, size_t size)
{
  std::vector<uint8_t> *temp = new std::vector<uint8_t> (data, data + size);

  piksi_log(LOG_INFO, "frame_handler acquiring lock");
  std::unique_lock<std::mutex> mlock(_mutex);
  piksi_log(LOG_INFO, "frame_handler lock acquired");
  _queue.push_back( temp );
  piksi_log(LOG_INFO, "frame_handler releasing lock");
  mlock.unlock();
  _cond.notify_one();
}

void RotatingLogger::process_frame()
{
  piksi_log(LOG_INFO, "process_frame");

  std::unique_lock<std::mutex> mlock(_mutex, std::defer_lock);
  while (!_closed) {
    piksi_log(LOG_INFO, "process_frame not closed, acquiring lock");
    mlock.lock();
    piksi_log(LOG_INFO, "process_frame lock acquired");
    while (_queue.empty()) {
      piksi_log(LOG_INFO, "process_frame queue empty, waiting");
      _cond.wait(mlock);
    }
    piksi_log(LOG_INFO, "process_frame woken up");

    if (!_dest_available) {
      // check imediately on startup for path availability. Subsequently, check
      // periodically
      if (_session_start_time.time_since_epoch().count() != 0 && get_time_passed() < _poll_period) {
        return;
      }
      piksi_log(LOG_INFO, "process_frame releasing lock to start new session");
      mlock.unlock();
      _session_start_time = std::chrono::steady_clock::now();
      if (!start_new_session()) {
        piksi_log(LOG_INFO, "start_new_session failed");
        return;
      }
      piksi_log(LOG_INFO, "process_frame session started, gimme lock back");
      mlock.lock();
      piksi_log(LOG_INFO, "process_frame session started, lock acquired");
    }
    if (!check_slice_time()) {
      return;
    }

    size_t num_written = 0;
    std::vector<uint8_t> *temp = _queue.front();
    size_t size = sizeof(std::vector<uint8_t>::value_type) * temp->size();
    if (_cur_file != nullptr) {
      num_written = fwrite(&temp[0], sizeof(std::vector<uint8_t>::value_type), temp->size(), _cur_file);
    }

    _queue.pop_front();
    delete temp;
    mlock.unlock();
    piksi_log(LOG_INFO, "process_frame unlocked");

    if (num_written != size) {
      // If drive is removed needs to close file imediately and not attempt to
      // open new file for a couple seconds to avoid locking mount
      close_current_file();
      _dest_available = false;
      // wait _poll_period to check drive again
      _session_start_time = std::chrono::steady_clock::now();
      piksi_log(LOG_INFO, "%d %d", num_written, size);
      log_msg(LOG_WARNING, std::string("Write to file failed: ") + strerror(errno));
    }
    _bytes_written += size;
  }
  piksi_log(LOG_INFO, "process_frame closed");
}

void RotatingLogger::update_dir(const std::string &out_dir)
{
  piksi_log(LOG_INFO, "update_dir");
  _out_dir = out_dir;
}

void RotatingLogger::update_fill_threshold(size_t disk_full_threshold)
{
  piksi_log(LOG_INFO, "update_fill_threshold");
  _disk_full_threshold = disk_full_threshold;
}

void RotatingLogger::update_slice_duration(size_t slice_duration)
{
  piksi_log(LOG_INFO, "update_slice_duration");
  _slice_duration = slice_duration;
}

RotatingLogger::RotatingLogger(const std::string &out_dir,
                               size_t slice_duration,
                               size_t poll_period,
                               size_t disk_full_threshold,
                               LogCall logging_callback)
  : _dest_available(false), _session_count(0), _minute_count(0), _out_dir(out_dir),
    _slice_duration(slice_duration), _poll_period(poll_period),
    _disk_full_threshold(disk_full_threshold), _logging_callback(logging_callback),
    // init to 0
    _session_start_time(), _cur_file(nullptr), _bytes_written(0)
{
  piksi_log(LOG_INFO, "starting thread");
  _thread = std::thread(&RotatingLogger::process_frame, this);
  piksi_log(LOG_INFO, "starting thread fin");
}

RotatingLogger::~RotatingLogger()
{
  piksi_log(LOG_INFO, "~RotatingLogger");
  close_current_file();
  _closed = true;
  piksi_log(LOG_INFO, "acquiring lock");
  std::unique_lock<std::mutex> mlock(_mutex, std::defer_lock);
  mlock.lock();
  piksi_log(LOG_INFO, "notifying");
  mlock.unlock();
  _cond.notify_one();
  piksi_log(LOG_INFO, "joining thread");
  _thread.join();
  piksi_log(LOG_INFO, "~RotatingLogger");
}

void RotatingLogger::test()
{
  piksi_log(LOG_INFO, "testing thread");
}
