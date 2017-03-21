/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "gtest/gtest.h"

extern "C" {
#include "libskylark.h"
}


namespace {

  TEST(configuration, load_uuid_channel) {
    EXPECT_TRUE(false);
  }

  TEST(configuration, load_device_uuid) {
    EXPECT_TRUE(false);
  }

  TEST(configuration, load_configuration) {
    EXPECT_TRUE(false);
  }

}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
