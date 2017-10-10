/*
 * Copyright (C) 2017 Swift Navigation Inc.
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

#include <gtest/gtest.h>

#include "whitelists.h"

extern "C" int whitelist_notify(void *context);

enum port {
  PORT_WHITESPACE,
  PORT_VALID,
  PORT_EMPTY,
  PORT_MAX
};

typedef struct {
  const char *name;
  char whitelist[256];
} port_whitelist_config_t;

static port_whitelist_config_t port_whitelist_config[PORT_MAX] = {
  { "whitespace", " \t\n\r\v" },
  { "valid", "72,74" },
  { "empty", "" },
};

// The fixture for testing class RotatingLogger.
class PiksiSystemDaemonTests : public ::testing::Test { };

TEST_F(PiksiSystemDaemonTests, Whitelist_whitespace) {

  system("rm -f /etc/whitespace_filter_out_config");

  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_WHITESPACE]));

  std::ifstream t("/etc/whitespace_filter_out_config");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  ASSERT_STREQ("", str.c_str());
}

TEST_F(PiksiSystemDaemonTests, Whitelist_valid) {

  system("rm -f /etc/valid_filter_out_config");

  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_VALID]));

  std::ifstream t("/etc/valid_filter_out_config");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  ASSERT_STREQ("48 1\n4a 1\n", str.c_str());
}

TEST_F(PiksiSystemDaemonTests, Whitelist_empty) {

  system("rm -f /etc/valid_filter_out_config");
  system("rm -f /etc/empty_filter_out_config");

  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_VALID]));
  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_EMPTY]));
  
  std::ifstream t("/etc/empty_filter_out_config");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  ASSERT_STREQ("", str.c_str());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
