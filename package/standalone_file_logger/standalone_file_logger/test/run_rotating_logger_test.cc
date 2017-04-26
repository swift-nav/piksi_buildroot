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

#include <math.h>
#include <sys/statvfs.h>
#include <gtest/gtest.h>
#include "rotating_logger.h"

static const char* out_dir = "./";

static bool check_disk_fill = false;

namespace {

static double GetDiskDiskUsage() {
  struct statvfs fs_stats;
  assert(statvfs(out_dir, &fs_stats) == 0);
  return double(fs_stats.f_blocks - fs_stats.f_bavail) /
         double(fs_stats.f_blocks) * 100.;
}

// The fixture for testing class RotatingLogger.
class RotatingLoggerTest : public ::testing::Test, public RotatingLogger {
 protected:

  static void log_call(int priority, const char *msg)
  {
    printf("rotating_logger - %d: %s\n", priority, msg);
  }

  RotatingLoggerTest() : RotatingLogger("", 0, 0, 100, &RotatingLoggerTest::log_call) {
    rm_wrapper("*.sbp");
  }

  ~RotatingLoggerTest() override { rm_wrapper("*.sbp"); }

  void rm_wrapper(const char* arg) {
    char cmd_buf[1024];
    snprintf(cmd_buf, sizeof(cmd_buf), "rm '%s'/%s 2> /dev/null", out_dir, arg);
    system(cmd_buf);
  }

  void init(const std::string& path, size_t slice_duration) {
    _out_dir = path;
    _slice_duration = slice_duration;
  }

  // invalidate current file pointer
  void SetNullFilePointer() { 
    close(_cur_file);
    _cur_file = -1;
  }

  void SetFillThreshold(size_t threshold) { _disk_full_threshold = threshold; }

  void SetOutputPath(const std::string& path) { _out_dir = path; }

  // make logger think time progressed
  void MoveStartTimeBack(size_t minutes_back) {
    _session_start_time -= std::chrono::minutes(minutes_back);
  }
};

/*
 * Test that log over works as expected
 */
TEST_F(RotatingLoggerTest, NormalOperation) {
  const size_t period = 5;
  init(out_dir, period);

  const char expected_files[4][15] = {"0001-00000.sbp", "0001-00005.sbp",
                                      "0001-00010.sbp", "0001-00015.sbp"};
  int expected_file_content[4][5];

  // Write 1 log a "minute" for 20 "minutes"
  for (int i = 0; i < 20; i++) {
    frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));
    MoveStartTimeBack(1);
    expected_file_content[i / 5][i % 5] = i;
  }

  // Check that log roll overs occured and each has correct data
  for (int i = 0; i < 4; i++) {
    char file_name_buf[1024];
    snprintf(file_name_buf, sizeof(file_name_buf), "%s/%s", out_dir,
             expected_files[i]);
    FILE* fd = fopen(file_name_buf, "rb");
    ASSERT_TRUE(fd != nullptr) << "Missing file " << expected_files[i];
    int data_buf[5];
    size_t read_len = fread(data_buf, 1, sizeof(data_buf), fd);
    fclose(fd);
    ASSERT_EQ(sizeof(data_buf), read_len) << "Missing data "
                                          << expected_files[i];
    ASSERT_TRUE(memcmp(data_buf, expected_file_content[i], sizeof(data_buf)) ==
                0)
        << "Data mismatch " << expected_files[i];
  }
}

/*
 * Check low disk threshold
 * This requires running the with a second CL arg and expects a loop mount
 * created with host_tests/rotating_logger/test_disk.sh
 */
TEST_F(RotatingLoggerTest, DiskCheck) {
  const size_t buff_size = 10240;
  const size_t disk_threshold = 10;

  if (!check_disk_fill) {
    printf("Skipping disk check\n");
    return;
  }

  char buff[buff_size];
  const size_t period = 1;
  init(out_dir, period);
  SetFillThreshold(disk_threshold);

  int fill_writes = 0;
  while (true) {
    double usage = GetDiskDiskUsage();
    printf("disk %f%%\n", usage);
    memset(buff, fill_writes % 256, sizeof(buff));
    frame_handler(reinterpret_cast<uint8_t*>(buff), sizeof(buff));
    MoveStartTimeBack(1);
    if (disk_threshold < usage) {
      break;
    }
    fill_writes++;
  }
  for (int i = 0; i <= fill_writes; i++) {
    snprintf(buff, sizeof(buff), "%s/0001-%05d.sbp", out_dir, i);
    FILE* fd = fopen(buff, "rb");
    if (i < fill_writes) {
      ASSERT_TRUE(fd != nullptr) << "Missing file " << i;
      fclose(fd);
    } else {
      ASSERT_TRUE(fd == nullptr) << "Extra file " << i;
    }
  }
}

/*
 * Check that logging can start when the target path is not initially available
 */
TEST_F(RotatingLoggerTest, StartDisconnected) {
  const size_t period = 5;
  init("/invalid", period);

  const char expected_files[15] = {"0001-00000.sbp"};
  int expected_file_content[1] = {1};

  int i = 0;
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));
  i++;
  SetOutputPath(out_dir);
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));

  char file_name_buf[1024];
  snprintf(file_name_buf, sizeof(file_name_buf), "%s/%s", out_dir,
           expected_files);
  FILE* fd = fopen(file_name_buf, "rb");
  ASSERT_TRUE(fd != nullptr) << "Missing file " << expected_files;
  int data_buf[1];
  size_t read_len = fread(data_buf, 1, sizeof(data_buf), fd);
  fclose(fd);
  ASSERT_EQ(sizeof(data_buf), read_len) << "Missing data " << expected_files;
  ASSERT_TRUE(memcmp(data_buf, expected_file_content, sizeof(data_buf)) == 0)
      << "Data mismatch " << expected_files;
}

/*
 * Check that logging works when the path goes in and out of availability
 */
TEST_F(RotatingLoggerTest, DisconnectReconnect) {
  const size_t period = 5;
  init(out_dir, period);

  const char expected_files[3][15] = {"0001-00000.sbp", "0002-00000.sbp",
                                      "0003-00000.sbp"};
  int expected_file_content[3] = {0, 2, 5};

  int i = 0;
  // 0001-00000.sbp <- 0
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));
  i++;
  SetNullFilePointer();
  // BAD FILE <- 1
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));
  i++;
  // 0002-00000.sbp <- 2
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));
  i++;
  SetOutputPath("/invalid");
  SetNullFilePointer();
  // BAD FILE <- 3
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));
  i++;
  // BAD PATH <- 4
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));
  i++;
  SetOutputPath(out_dir);
  // 0003-00000.sbp <- 5
  frame_handler(reinterpret_cast<uint8_t*>(&i), sizeof(int));

  for (i = 0; i < 3; i++) {
    char file_name_buf[1024];
    snprintf(file_name_buf, sizeof(file_name_buf), "%s/%s", out_dir,
             expected_files[i]);
    FILE* fd = fopen(file_name_buf, "rb");
    ASSERT_TRUE(fd != nullptr) << "Missing file " << expected_files[i];
    int data_buf[1];
    size_t read_len = fread(data_buf, 1, sizeof(data_buf), fd);
    fclose(fd);
    ASSERT_EQ(sizeof(data_buf), read_len) << "Missing data "
                                          << expected_files[i];
    ASSERT_TRUE(
        memcmp(data_buf, &(expected_file_content[i]), sizeof(data_buf)) == 0)
        << "Data mismatch " << expected_files[i];
  }
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc >= 2) {
    out_dir = argv[1];
  }
  if (argc >= 3) {
    check_disk_fill = true;
  }

  return RUN_ALL_TESTS();
}
