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

#include <libpiksi_tests.h>

#define PROGRAM_NAME "libpiksi_tests"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);
  auto ret = RUN_ALL_TESTS();
  logging_deinit();
  return ret;
}
