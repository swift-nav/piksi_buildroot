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
#include "fio_debug.h"

#define PROGRAM_NAME "sbp_fileio_daemon_tests"

class SbpFileioDaemonTests : public ::testing::Test { };

TEST_F(SbpFileioDaemonTests, basic)
{
  path_validator_t *ctx = path_validator_create(NULL);
  ASSERT_TRUE( ctx != NULL );

  ASSERT_TRUE( path_validator_allow_path(ctx, "/fake_data/") );

  ASSERT_TRUE( path_validator_check(ctx, "/fake_data/") );

  ASSERT_TRUE( path_validator_check(ctx, "/fake_data/foobar") );
  ASSERT_FALSE( path_validator_check(ctx, "/usr/bin/foobar") );

  ASSERT_TRUE( path_validator_allowed_count(ctx) == 1 );

  ASSERT_TRUE( path_validator_allow_path(ctx, "/fake_persist/blah/") );
  ASSERT_TRUE( path_validator_check(ctx, "/fake_persist/blah/blahblah.txt") );

  ASSERT_TRUE( path_validator_check(ctx, "fake_persist/blah/blahblah.txt") );

  ASSERT_TRUE( path_validator_allowed_count(ctx) == 2 );

  const char* base_paths = path_validator_base_paths(ctx);
  fprintf(stderr, "base_paths: %s\n", base_paths);

  ASSERT_TRUE( strcmp(base_paths, "/fake_persist/blah/,/fake_data/") == 0 );

  path_validator_destroy(&ctx);
  ASSERT_EQ( NULL, ctx );
}

TEST_F(SbpFileioDaemonTests, overflow)
{
  path_validator_cfg_t cfg = (path_validator_cfg_t) { .print_buf_size = 54 };
  path_validator_t *ctx = path_validator_create(&cfg);

  ASSERT_TRUE( ctx != NULL );

  ASSERT_TRUE( path_validator_allow_path(ctx, "/fake_data/") );         //   11 + 1 (path plus comma)
  ASSERT_TRUE( path_validator_allow_path(ctx, "/fake_persist/blah/") ); //   19 + 1 (path plus comma)
  ASSERT_TRUE( path_validator_allow_path(ctx, "/also_fake/") );         //   11     (path)
                                                                        //      + 1 (null)
                                                                        // = 44 bytes

  const char* base_paths = path_validator_base_paths(ctx);
  fprintf(stderr, "base_paths: %s\n", base_paths);

  ASSERT_TRUE( path_validator_allowed_count(ctx) == 3 );
  ASSERT_TRUE( strcmp(base_paths, "/also_fake/,/fake_persist/blah/,/fake_data/") == 0 );

  ASSERT_TRUE( path_validator_allow_path(ctx, "/another_fake_path/") );
  ASSERT_TRUE( path_validator_allowed_count(ctx) == 4 );

  base_paths = path_validator_base_paths(ctx);
  fprintf(stderr, "base_paths: %s\n", base_paths);

  // Paths that don't fit will be truncated
  ASSERT_TRUE( strcmp(base_paths, "/another_fake_path/,/also_fake/,...") == 0 );

  path_validator_destroy(&ctx);
  ASSERT_EQ( NULL, ctx );
}

TEST_F(SbpFileioDaemonTests, overflow_allow)
{
  path_validator_t *ctx = path_validator_create(NULL);
  ASSERT_TRUE( ctx != NULL );

  char really_big_path[4096] = {0};
  memset(really_big_path, 'x', sizeof(really_big_path) - 1);

  ASSERT_TRUE( path_validator_allow_path(ctx, really_big_path) );

  char really_big_path2[4097] = {0};
  memset(really_big_path2, 'x', sizeof(really_big_path2) - 1);

  ASSERT_FALSE( path_validator_allow_path(ctx, really_big_path2) );

  path_validator_destroy(&ctx);
  ASSERT_EQ( NULL, ctx );
}

int main(int argc, char** argv)
{
  fio_debug = true;

  ::testing::InitGoogleTest(&argc, argv);

  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);

  auto ret = RUN_ALL_TESTS();

  logging_deinit();

  return ret;
}
