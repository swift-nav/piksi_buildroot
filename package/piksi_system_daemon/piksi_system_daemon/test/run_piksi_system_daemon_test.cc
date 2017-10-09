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

#include <gtest/gtest.h>

#include "whitelists.h"

extern "C" int whitelist_notify(void *context);

enum port {
  PORT_WHITESPACE,
  PORT_EMPTY,
  PORT_VALID,
  PORT_MAX
};

typedef struct {
  const char *name;
  char whitelist[256];
} port_whitelist_config_t;

static port_whitelist_config_t port_whitelist_config[PORT_MAX] = {
  { "uart0", " \t\n\r\v" },
  { "uart0", "72,74" },
  { "uart0", "" },
};

// The fixture for testing class RotatingLogger.
class PiksiSystemDaemonTests : public ::testing::Test { };

TEST_F(PiksiSystemDaemonTests, Whitelist_whitespace) {
  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_WHITESPACE]));
}

TEST_F(PiksiSystemDaemonTests, Whitelist_valid) {
  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_VALID]));
}

TEST_F(PiksiSystemDaemonTests, Whitelist_empty) {
  ASSERT_EQ(0, whitelist_notify(&port_whitelist_config[PORT_EMPTY]));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
