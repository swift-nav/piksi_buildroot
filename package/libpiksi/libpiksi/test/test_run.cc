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

#include <gtest/gtest.h>

#include <libpiksi_tests.h>

#include <libpiksi/util.h>

TEST_F(LibpiksiTests, runWithStdinFileTests)
{
  const char *stdin_file_name = "/tmp/stdin.txt";
  FILE *file_ptr = fopen(stdin_file_name, "w");

  EXPECT_FALSE(file_ptr == NULL);

  const char *test_str = "string literal";
  fputs(test_str, file_ptr);
  fclose(file_ptr);

  // Valid with stdin file
  {
    const char *cmd = "cat";
    const char *const argv[2] = {"cat", NULL};
    char stdout_str[strlen(test_str) + 16] = {0};

    EXPECT_EQ(0,
              run_with_stdin_file(stdin_file_name,
                                  cmd,
                                  const_cast<char *const *>(argv),
                                  stdout_str,
                                  sizeof(stdout_str)));
    EXPECT_STREQ(stdout_str, test_str);
  }

  // Valid without stdin file
  {
    const char *cmd = "cat";
    const char *const argv[3] = {"cat", stdin_file_name, NULL};
    char stdout_str[strlen(test_str) + 16] = {0};

    EXPECT_EQ(0,
              run_with_stdin_file(stdin_file_name,
                                  cmd,
                                  const_cast<char *const *>(argv),
                                  stdout_str,
                                  sizeof(stdout_str)));
    EXPECT_STREQ(stdout_str, test_str);
  }

  // Valid with stdin file, too small output buffer
  {
    const char *cmd = "cat";
    const char *const argv[2] = {"cat", NULL};
    char stdout_str[strlen(test_str) - 4] = {0};

    EXPECT_EQ(0,
              run_with_stdin_file(stdin_file_name,
                                  cmd,
                                  const_cast<char *const *>(argv),
                                  stdout_str,
                                  sizeof(stdout_str)));
    EXPECT_EQ(0, strncmp(stdout_str, test_str, strlen(stdout_str)));
  }

  // Valid with stdin file, exact output buffer
  {
    const char *cmd = "cat";
    const char *const argv[2] = {"cat", NULL};
    char stdout_str[strlen(test_str) + 1] = {0};

    EXPECT_EQ(0,
              run_with_stdin_file(stdin_file_name,
                                  cmd,
                                  const_cast<char *const *>(argv),
                                  stdout_str,
                                  sizeof(stdout_str)));
    EXPECT_STREQ(stdout_str, test_str);
  }

  // Clean up test file
  if (remove(stdin_file_name)) {
    std::cout << "Failed to clean up " << stdin_file_name << std::endl;
  }
}
