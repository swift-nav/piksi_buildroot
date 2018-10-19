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

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include <libpiksi_tests.h>


TEST_F(LibpiksiTests, snprintfTests)
{
  // Valid
  {
    char str[32];
    const char *test_str = "string literal";
    snprintf_assert(str, sizeof(str), "%s", test_str);
    EXPECT_TRUE(0 == strcmp(str, test_str));
    snprintf_warn(str, sizeof(str), "%s", test_str);
    EXPECT_TRUE(0 == strcmp(str, test_str));
  }

  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Buffer too small
  {
    char str[2] = {0};
    const char *test_str = "Too long string literal";
    EXPECT_DEATH(snprintf_assert(str, sizeof(str), "%s", test_str), "");

    EXPECT_FALSE(snprintf_warn(str, sizeof(str), "%s", test_str));
    EXPECT_STREQ(str, "T");
  }

  // NULL pointers
  {
    char str[32] = {0};
    const char *test_str = "string literal";

    EXPECT_DEATH(snprintf_assert(NULL, sizeof(str), "%s", test_str), "");
    EXPECT_DEATH(snprintf_warn(NULL, sizeof(str), "%s", test_str), "");

    EXPECT_DEATH(snprintf_assert(str, sizeof(str), NULL, test_str), "");
    EXPECT_DEATH(snprintf_warn(str, sizeof(str), NULL, test_str), "");
  }
}

TEST_F(LibpiksiTests, fileReadStringTests)
{
  const char *test_file = "/tmp/test.txt";
  FILE *file_ptr = fopen(test_file, "w");

  ASSERT_FALSE(file_ptr == NULL);

  const char *test_str = "string literal";
  fputs(test_str, file_ptr);
  fclose(file_ptr);

  // Valid
  {
    char str[16] = {0};
    EXPECT_EQ(0, file_read_string(test_file, str, sizeof(str)));
    EXPECT_STREQ(str, test_str);
  }

  // NULL pointers
  {
    char str[16] = {0};

    EXPECT_EQ(-1, file_read_string(NULL, str, sizeof(str)));
    EXPECT_TRUE(0 == strcmp(str, ""));

    EXPECT_EQ(-1, file_read_string(test_file, NULL, sizeof(str)));
    EXPECT_STREQ(str, "");
  }

  // Invalid test_file
  {
    char str[16] = {0};

    EXPECT_EQ(-1, file_read_string("this-file-does-not-exist", str, sizeof(str)));
    EXPECT_STREQ(str, "");
  }

  // Truncation
  {
    char str[4] = {0};
    char test_str_sub[4] = {0};
    memcpy(test_str_sub, test_str, 3);

    EXPECT_EQ(0, file_read_string(test_file, str, sizeof(str)));
    EXPECT_STREQ(str, test_str_sub);
  }
}

TEST_F(LibpiksiTests, strDigitsOnlyTests)
{
  // Valid
  {
    EXPECT_TRUE(str_digits_only("123"));
    EXPECT_TRUE(str_digits_only("000"));
    EXPECT_TRUE(str_digits_only("010"));
    EXPECT_TRUE(str_digits_only("900"));
  }

  // Invalid
  {
    EXPECT_FALSE(str_digits_only("abc"));
    EXPECT_FALSE(str_digits_only("1b3"));
    EXPECT_FALSE(str_digits_only("a2c"));
    EXPECT_FALSE(str_digits_only("1.2"));
    EXPECT_FALSE(str_digits_only("1,2"));
    EXPECT_FALSE(str_digits_only("$32"));
    EXPECT_FALSE(str_digits_only("13:37"));
    EXPECT_FALSE(str_digits_only(".23"));
  }

  // NULL pointer
  {
    EXPECT_FALSE(str_digits_only(NULL));
  }

  // Empty string
  {
    EXPECT_FALSE(str_digits_only(""));
  }
}

#define PROGRAM_NAME "libpiksi_tests"

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);

  auto ret = RUN_ALL_TESTS();
  logging_deinit();

  return ret;
}
