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

extern "C" {
#include "whitelists.h"
}

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
class PortsDaemonTests : public ::testing::Test { };

TEST_F(PortsDaemonTests, Whitelist_whitespace) {

  system("rm -f /etc/filter_out_config/whitespace");

  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_WHITESPACE]));

  std::ifstream t("/etc/whitespace_filter_out_config");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  ASSERT_STREQ("", str.c_str());
}

TEST_F(PortsDaemonTests, Whitelist_valid) {

  system("rm -f /etc/filter_out_config/valid");

  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_VALID]));

  std::ifstream t("/etc/filter_out_config/valid");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  ASSERT_STREQ("48 1\n4a 1\n", str.c_str());
}

TEST_F(PortsDaemonTests, Whitelist_empty) {

  system("rm -f /etc/filter_out_config/valid");
  system("rm -f /etc/filter_out_config/empty");

  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_VALID]));
  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_EMPTY]));

  std::ifstream t("/etc/empty_filter_out_config");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  ASSERT_STREQ("", str.c_str());
}

int main(int argc, char** argv) {
  system("mkdir -p /etc/filter_out_config");
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
