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

#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>

#include <gtest/gtest.h>

#include <libpiksi/logging.h>

#include "path_validator.h"
#include "sbp_fileio.h"

#define PROGRAM_NAME "sbp_fileio_daemon_tests"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

static size_t pack_write_message(u32 sequence,
                                 u32 write_offset,
                                 const char *filename,
                                 const char *data,
                                 u8 **packed_msg)
{
  static u8 msg_buffer[SBP_FRAMING_MAX_PAYLOAD_SIZE];

  msg_fileio_write_req_t msg = {.sequence = sequence, .offset = write_offset};

  size_t offset = 0;
  memcpy(&msg_buffer[offset], &msg, sizeof(msg));

  offset += sizeof(msg);
  memcpy(&msg_buffer[offset], filename, strlen(filename) + 1);

  offset += strlen(filename) + 1;
  memcpy(&msg_buffer[offset], data, strlen(data));

  offset += strlen(data);

  *packed_msg = msg_buffer;

  return offset;
}

class SbpFileioDaemonTests : public ::testing::Test {
 protected:
  SbpFileioDaemonTests()
  {
    system("rm -f fileio_test.*.bin");
  }
  ~SbpFileioDaemonTests() override
  {
    system("rm -f fileio_test.*.bin");
  }
};

TEST_F(SbpFileioDaemonTests, basic)
{
  path_validator_t *ctx = path_validator_create(NULL);
  ASSERT_TRUE(ctx != NULL);

  ASSERT_TRUE(path_validator_allow_path(ctx, "/fake_data/"));

  ASSERT_TRUE(path_validator_check(ctx, "/fake_data/"));

  ASSERT_TRUE(path_validator_check(ctx, "/fake_data/foobar"));
  ASSERT_FALSE(path_validator_check(ctx, "/usr/bin/foobar"));

  ASSERT_TRUE(path_validator_allowed_count(ctx) == 1);

  ASSERT_TRUE(path_validator_allow_path(ctx, "/fake_persist/blah/"));
  ASSERT_TRUE(path_validator_check(ctx, "/fake_persist/blah/blahblah.txt"));

  ASSERT_TRUE(path_validator_check(ctx, "fake_persist/blah/blahblah.txt"));

  ASSERT_TRUE(path_validator_allowed_count(ctx) == 2);

  const char *base_paths = path_validator_base_paths(ctx);
  fprintf(stderr, "base_paths: %s\n", base_paths);

  ASSERT_TRUE(strcmp(base_paths, "/fake_persist/blah/,/fake_data/") == 0);

  path_validator_destroy(&ctx);
  ASSERT_EQ(NULL, ctx);
}

TEST_F(SbpFileioDaemonTests, overflow)
{
  path_validator_cfg_t cfg = (path_validator_cfg_t){.print_buf_size = 54};
  path_validator_t *ctx = path_validator_create(&cfg);

  ASSERT_TRUE(ctx != NULL);

  ASSERT_TRUE(path_validator_allow_path(ctx, "/fake_data/"));         //   11 + 1 (path plus comma)
  ASSERT_TRUE(path_validator_allow_path(ctx, "/fake_persist/blah/")); //   19 + 1 (path plus comma)
  ASSERT_TRUE(path_validator_allow_path(ctx, "/also_fake/"));         //   11     (path)
                                                                      //      + 1 (null)
                                                                      // = 44 bytes

  const char *base_paths = path_validator_base_paths(ctx);
  fprintf(stderr, "base_paths: %s\n", base_paths);

  ASSERT_TRUE(path_validator_allowed_count(ctx) == 3);
  ASSERT_TRUE(strcmp(base_paths, "/also_fake/,/fake_persist/blah/,/fake_data/") == 0);

  ASSERT_TRUE(path_validator_allow_path(ctx, "/another_fake_path/"));
  ASSERT_TRUE(path_validator_allowed_count(ctx) == 4);

  base_paths = path_validator_base_paths(ctx);
  fprintf(stderr, "base_paths: %s\n", base_paths);

  // Paths that don't fit will be truncated
  ASSERT_TRUE(strcmp(base_paths, "/another_fake_path/,/also_fake/,...") == 0);

  path_validator_destroy(&ctx);
  ASSERT_EQ(NULL, ctx);
}

TEST_F(SbpFileioDaemonTests, overflow_allow)
{
  path_validator_t *ctx = path_validator_create(NULL);
  ASSERT_TRUE(ctx != NULL);

  char really_big_path[4096] = {0};
  memset(really_big_path, 'x', sizeof(really_big_path) - 1);

  ASSERT_TRUE(path_validator_allow_path(ctx, really_big_path));

  char really_big_path2[4097] = {0};
  memset(really_big_path2, 'x', sizeof(really_big_path2) - 1);

  ASSERT_FALSE(path_validator_allow_path(ctx, really_big_path2));

  path_validator_destroy(&ctx);
  ASSERT_EQ(NULL, ctx);
}

TEST_F(SbpFileioDaemonTests, test_write)
{
  const char test_data[] = "test data";
  const char filename[] = "fileio_test.0.bin";

  size_t test_data_len = strlen(test_data);

  {
    u8 *write_msg = nullptr;
    size_t len = pack_write_message(123, 0, filename, test_data, &write_msg);

    size_t write_count = 0;
    sbp_fileio_write((msg_fileio_write_req_t *)write_msg, len, &write_count);

    ASSERT_EQ(write_count, strlen(test_data));
  }
  {
    u8 *write_msg = nullptr;
    size_t len = pack_write_message(123, test_data_len, filename, test_data, &write_msg);

    size_t write_count = 0;
    sbp_fileio_write((msg_fileio_write_req_t *)write_msg, len, &write_count);

    ASSERT_EQ(write_count, strlen(test_data));
  }

  sbp_fileio_flush();

  char buffer[128] = {0};

  FILE *fp = fopen("fileio_test.0.bin", "r");
  fread(buffer, 1, sizeof(buffer), fp);

  ASSERT_TRUE(memcmp(test_data, buffer, test_data_len) == 0);
  ASSERT_TRUE(memcmp(test_data, buffer + test_data_len, test_data_len) == 0);

  fclose(fp);
}

TEST_F(SbpFileioDaemonTests, test_write_random)
{
  const char test_data[] = "test data";
  const char test_data_cmp[] = "ttest data";
  const char filename[] = "fileio_test.0.bin";

  size_t test_data_len = strlen(test_data);

  {
    u8 *write_msg = nullptr;
    size_t len = pack_write_message(123, 0, filename, test_data, &write_msg);

    size_t write_count = 0;
    sbp_fileio_write((msg_fileio_write_req_t *)write_msg, len, &write_count);

    ASSERT_EQ(write_count, strlen(test_data));
  }
  // Next write is "random" because the wrote more than 1 byte
  {
    u8 *write_msg = nullptr;
    size_t len = pack_write_message(123, 1, filename, test_data, &write_msg);

    size_t write_count = 0;
    sbp_fileio_write((msg_fileio_write_req_t *)write_msg, len, &write_count);

    ASSERT_EQ(write_count, strlen(test_data));
  }

  sbp_fileio_flush();

  char buffer[128] = {0};

  FILE *fp = fopen("fileio_test.0.bin", "r");
  fread(buffer, 1, sizeof(buffer), fp);

  ASSERT_TRUE(memcmp(test_data_cmp, buffer, strlen(test_data_cmp)) == 0);

  fclose(fp);
}

int main(int argc, char **argv)
{
  fio_debug = true;

  ::testing::InitGoogleTest(&argc, argv);

  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);

  path_validator_t *pv = path_validator_create(NULL);
  path_validator_allow_path(pv, "/");

  sbp_fileio_setup_path_validator(pv, false, false);

  auto ret = RUN_ALL_TESTS();

  logging_deinit();

  return ret;
}
