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

#include <libpiksi/sha256.h>

TEST_F(LibpiksiTests, sha256Tests)
{
  const char *test_file = "/tmp/test_sha.txt";

  // File not present
  {
    char sha[SHA256SUM_LEN] = {0};
    EXPECT_NE(0, sha256sum_file(test_file, sha, sizeof(sha)));
  }

  FILE *file_ptr = fopen(test_file, "w");

  ASSERT_FALSE(file_ptr == NULL);

  const char *test_str = "string literal";
  fputs(test_str, file_ptr);
  fclose(file_ptr);

  const char *expected = "750bb977005f3d8782afd1e7fd0b580233828383f8f6834b57f34a52e65e3d3c";
  const char *not_expected = "750bb977005f3d8782afd1e7fd0b580233828383f8f6834b57f34a52e65e3d3d";

  // Valid
  {
    char sha[SHA256SUM_LEN] = {0};
    EXPECT_EQ(0, sha256sum_file(test_file, sha, sizeof(sha)));
    EXPECT_EQ(0, sha256sum_cmp(expected, sha));
    EXPECT_NE(0, sha256sum_cmp(not_expected, sha));
  }

  // Clean up test file
  if (remove(test_file)) {
    std::cout << "Failed to clean up " << test_file << std::endl;
  }
}
