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

#include <libpiksi/version.h>

#define DEVICE_FW_VERSION_DIR_PATH_1 "/img_tbl/"
#define DEVICE_FW_VERSION_DIR_PATH_2 "boot/"
#define DEVICE_FW_VERSION_DIR_PATH_FULL DEVICE_FW_VERSION_DIR_PATH_1 DEVICE_FW_VERSION_DIR_PATH_2
#define DEVICE_FW_VERSION_FILE_PATH DEVICE_FW_VERSION_DIR_PATH_FULL "name"

TEST_F(LibpiksiTests, versionTests)
{
  // Version file not present
  {
    piksi_version_t ver = {0};
    EXPECT_NE(0, version_current_get(&ver));
  }

  ASSERT_EQ(0, mkdir(DEVICE_FW_VERSION_DIR_PATH_1, 0777));
  ASSERT_EQ(0, mkdir(DEVICE_FW_VERSION_DIR_PATH_FULL, 0777));

  FILE *file_ptr = fopen(DEVICE_FW_VERSION_FILE_PATH, "w");

  ASSERT_FALSE(file_ptr == NULL);

  const char *test_str = "DEV v2.1.4";
  fputs(test_str, file_ptr);
  fclose(file_ptr);

  // Version file present
  {
    piksi_version_t ver = {0};
    EXPECT_EQ(0, version_current_get(&ver));

    char ver_str[16] = {0};
    EXPECT_EQ(0, version_current_get_str(ver_str, sizeof(ver_str)));
    EXPECT_STREQ(ver_str, test_str);
  }

  // Clean up test file
  if (remove(DEVICE_FW_VERSION_FILE_PATH)) {
    std::cout << "Failed to clean up " << DEVICE_FW_VERSION_FILE_PATH << std::endl;
  }

  if (rmdir(DEVICE_FW_VERSION_DIR_PATH_FULL)) {
    std::cout << "Failed to clean up " << DEVICE_FW_VERSION_DIR_PATH_FULL << std::endl;
  }

  if (rmdir(DEVICE_FW_VERSION_DIR_PATH_1)) {
    std::cout << "Failed to clean up " << DEVICE_FW_VERSION_DIR_PATH_1 << std::endl;
  }

  // Version comparison
  {
    piksi_version_t ver1 = {.marketing = 1, .major = 2, .patch = 3};
    piksi_version_t ver2 = {.marketing = 2, .major = 3, .patch = 4};

    EXPECT_GT(0, version_cmp(&ver1, &ver2));
    EXPECT_LT(0, version_cmp(&ver2, &ver1));
    EXPECT_EQ(0, version_cmp(&ver1, &ver1));
  }

  // Version parsing
  {
    piksi_version_t ver = {0};

    // Valid
    EXPECT_EQ(0, version_parse_str(test_str, &ver));
    EXPECT_EQ(2, ver.marketing);
    EXPECT_EQ(1, ver.major);
    EXPECT_EQ(4, ver.patch);

    EXPECT_EQ(0, version_parse_str("2.1.4", &ver));
    EXPECT_EQ(2, ver.marketing);
    EXPECT_EQ(1, ver.major);
    EXPECT_EQ(4, ver.patch);

    EXPECT_EQ(0, version_parse_str("v2.1.4", &ver));
    EXPECT_EQ(2, ver.marketing);
    EXPECT_EQ(1, ver.major);
    EXPECT_EQ(4, ver.patch);

    // Invalid
    EXPECT_NE(0, version_parse_str("x.y.z", &ver));
    EXPECT_NE(0, version_parse_str("2.1.4z", &ver));
    EXPECT_NE(0, version_parse_str("2.y1.4", &ver));
    EXPECT_NE(0, version_parse_str("2x.y1.4", &ver));
  }
}
