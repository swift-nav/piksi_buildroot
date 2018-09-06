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

#define PROGRAM_NAME "sbp_fileio_daemon_tests"

class SbpFileioDaemonTests : public ::testing::Test { };

TEST_F(SbpFileioDaemonTests, basic)
{
  path_validator_t *ctx = path_validator_create();
  ASSERT_TRUE( ctx != NULL );

  path_validator_allow_path(ctx, "/fake_data");

  ASSERT_TRUE( path_validator_check(ctx, "/fake_data/foobar") );
  ASSERT_FALSE( path_validator_check(ctx, "/usr/bin/foobar") );

  ASSERT_TRUE( path_validator_allowed_count(ctx) == 1 );

  path_validator_allow_path(ctx, "/fake_persist/blah");
  ASSERT_TRUE( path_validator_check(ctx, "/fake_persist/blah/blahblah.txt") );

  ASSERT_TRUE( path_validator_allowed_count(ctx) == 2 );

  path_validator_destroy(&ctx);
}

int main(int argc, char** argv) {

  ::testing::InitGoogleTest(&argc, argv);

  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);

  auto ret = RUN_ALL_TESTS();

  logging_deinit();

  return ret;
}