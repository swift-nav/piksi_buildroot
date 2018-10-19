/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Dev <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>

#include <gtest/gtest.h>

#include <libpiksi/logging.h>

#include "resmon_common.h"

#define PROGRAM_NAME "resmond_tests"

ssize_t get_file_size(const char *filename)
{
  struct stat st;
  if (stat(filename, &st) != 0) {
    return -1;
  }
  return st.st_size;
}

void get_sz_data(const char *filename, char *buffer, size_t buflen)
{
  ssize_t filesize = get_file_size(filename);

  EXPECT_GE(filesize, 0);
  EXPECT_LE(filesize, buflen);

  int fd = open(filename, O_RDONLY, 0);
  EXPECT_GE(fd, 0);

  void *sz_data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
  EXPECT_NE(sz_data, MAP_FAILED);

  memset(buffer, 0, buflen);
  memcpy(buffer, sz_data, filesize);

  munmap(sz_data, filesize);
}

class ResmondTests : public ::testing::Test {
};

TEST_F(ResmondTests, count_lines)
{
  int lines = -1;

  lines = count_lines("/data/resource_monitor/lines.txt");
  ASSERT_EQ(lines, 7);

  lines = count_lines("/data/resource_monitor/empty.txt");
  ASSERT_EQ(lines, 0);
}


TEST_F(ResmondTests, count_sz_lines)
{
  int lines = -1;

  lines = count_sz_lines("");
  ASSERT_EQ(lines, 0);

  lines = count_sz_lines("\n");
  ASSERT_EQ(lines, 1);

  lines = count_sz_lines("1\n2\n3\n");
  ASSERT_EQ(lines, 3);

  lines = count_sz_lines("1\n2\n3");
  ASSERT_EQ(lines, 3);

  char buffer[1024] = {0};
  get_sz_data("/data/resource_monitor/lines.txt", buffer, sizeof(buffer));

  lines = count_sz_lines(buffer);
  ASSERT_EQ(lines, 7);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);

  auto ret = RUN_ALL_TESTS();

  logging_deinit();

  return ret;
}
