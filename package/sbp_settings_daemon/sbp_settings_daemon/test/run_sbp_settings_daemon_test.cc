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
#include <iostream>

#include <gtest/gtest.h>

#define BUFSIZE 256

struct setting {
  char section[BUFSIZE];
  char name[BUFSIZE];
  char type[BUFSIZE];
  char value[BUFSIZE];
  struct setting *next;
  bool dirty;
};

extern "C" void settings_register(struct setting *setting);

// clang-format off
static setting setting_empty_uart0 = {
  /* section = */ "uart0", /* name = */ "enabled_sbp_messages", /* type  = */ "",
  /* value   = */   "",    /* next = */ NULL,                   /* dirty = */ false,
};
// clang-format on

class SbpSettingsDaemonTests : public ::testing::Test {
};

TEST_F(SbpSettingsDaemonTests, empty_ini_field)
{

  system("rm -rf /persistent");
  system("mkdir /persistent");

  std::ofstream config_ini("/persistent/config.ini");
  std::string config_ini_content("[uart0]\nenabled_sbp_messages=\n");

  config_ini << config_ini_content;
  config_ini.close();

  settings_register(&setting_empty_uart0);

  ASSERT_TRUE(setting_empty_uart0.dirty);
  ASSERT_STREQ("", setting_empty_uart0.value);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
