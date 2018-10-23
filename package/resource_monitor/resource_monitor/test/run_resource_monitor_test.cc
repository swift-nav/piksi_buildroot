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

static unsigned long line_func_next_line = 10;

bool line_func(const char *line)
{
  (void)line;
  unsigned long current = 0;

  EXPECT_TRUE(strtoul_all(10, line, &current));
  EXPECT_EQ(current, line_func_next_line);

  line_func_next_line += 10;

  return true;
}

TEST_F(ResmondTests, foreach_line)
{
  line_func_next_line = 10;

  const char lines[] = "10\n20\n30\n40\n50\n60\n70";
  const char lines2[] = "\n80\n90\n100\n";

  char buf[8];
  char leftover_buf[4];

  leftover_t leftover = {.buf = leftover_buf};

  ssize_t consumed = foreach_line(lines, &leftover, line_func);

  ASSERT_EQ(consumed, sizeof(lines) - 1);
  ASSERT_EQ(leftover.size, 2);
  ASSERT_EQ(leftover.line_count, 6);

  size_t leftover_size = leftover.size;
  consumed = foreach_line(lines2, &leftover, line_func);

  ASSERT_EQ(consumed, leftover_size + sizeof(lines2) - 1);
  ASSERT_EQ(leftover.size, 0);
  ASSERT_EQ(leftover.line_count, 4);

  const char lines3[] = "110";

  consumed = foreach_line(lines3, &leftover, line_func);

  ASSERT_EQ(consumed, 3);
  ASSERT_EQ(leftover.size, 3);
  ASSERT_EQ(leftover.line_count, 0);

  const char lines4[] = "\n120\n130\n";

  leftover_size = leftover.size;
  consumed = foreach_line(lines4, &leftover, line_func);

  ASSERT_EQ(consumed, leftover_size + sizeof(lines4) - 1);
  ASSERT_EQ(leftover.size, 0);
  ASSERT_EQ(leftover.line_count, 3);
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
