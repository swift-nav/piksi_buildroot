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

static const char *test_str = "string literal";

ssize_t buffer_func(char *buffer, size_t length, void *context)
{
  size_t *offset = (size_t *)context;
  buffer[length] = '\0';

  EXPECT_TRUE(buffer[0] == test_str[*offset]);
  *offset += 1;

  return (ssize_t)1;
}

TEST_F(LibpiksiTests, runWithStdinFileTests)
{
  const char *stdin_file_name = "/tmp/stdin.txt";
  FILE *file_ptr = fopen(stdin_file_name, "w");

  EXPECT_FALSE(file_ptr == NULL);

  fputs(test_str, file_ptr);
  fclose(file_ptr);

  // Valid with stdin file
  {
    const char *cmd = "/bin/cat";
    const char *const argv[2] = {"/bin/cat", NULL};
    char stdout_str[strlen(test_str) + 64] = {0};

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

  pipeline_t *r = create_pipeline();
  EXPECT_NE(r, nullptr);

  r = r->cat(r, stdin_file_name);
  EXPECT_NE(r, nullptr);
  EXPECT_FALSE(r->is_nil(r));

  r = r->pipe(r);
  EXPECT_NE(r, nullptr);
  EXPECT_FALSE(r->is_nil(r));

  r = r->call(r, "grep", (const char *const[]){"grep", "literal", NULL});
  EXPECT_NE(r, nullptr);
  EXPECT_FALSE(r->is_nil(r));

  r = r->wait(r);
  EXPECT_NE(r, nullptr);
  EXPECT_FALSE(r->is_nil(r));
  EXPECT_EQ(r->exit_code, 0);

  r = r->destroy(r);

  // Use run_command, specify a buffer function
  {
    const char *const argv[] = {"cat", stdin_file_name, NULL};

    char buffer[128] = {0};

    size_t offset = 0;

    run_command_t run_config = {
      .input = NULL,
      .argv = argv,
      .buffer = buffer,
      .length = sizeof(buffer) - 1,
      .func = buffer_func,
      .context = &offset,
    };

    EXPECT_EQ(0, run_command(&run_config));
    EXPECT_EQ(offset, strlen(test_str));
  }

  // Clean up test file
  if (remove(stdin_file_name)) {
    std::cout << "Failed to clean up " << stdin_file_name << std::endl;
  }
}
