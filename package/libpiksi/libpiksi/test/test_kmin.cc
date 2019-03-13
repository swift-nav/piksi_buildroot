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

#include <cstdlib>

#include <gtest/gtest.h>

#include <libpiksi/kmin.h>

#include <libpiksi_tests.h>

#define KMIN_SMALL_SIZE 100
#define KMIN_MEDIUM_SIZE 20000
#define KMIN_LARGE_SIZE 200000

#define STR_SIZE 64
#define KMIN_COUNT 10

#define IDENT_TEMPLATE "This is a key with a value of %zu"

const bool run_long_tests = false;

#define SKIP_LONG_TEST()                        \
  if (!run_long_tests) {                        \
    fprintf(stderr, "skipping long test...\n"); \
    return;                                     \
  }

static void do_kmin_test(size_t kmin_size_, size_t kmin_count, bool invert, bool skip_even)
{
  kmin_t *kmin = kmin_create(kmin_size_);
  ASSERT_NE(kmin, nullptr);

  char **strings = (char **)calloc(kmin_size_, sizeof(char *));

  for (size_t x = 0; x < kmin_size_; x++) {
    strings[x] = (char *)calloc(STR_SIZE, sizeof(char));
  }

  for (size_t x = 0; x < kmin_size_; x++) {

    size_t index = rand() % kmin_size_;
    while (strings[index][0] != '\0')
      index = rand() % kmin_size_;

    snprintf(strings[index], STR_SIZE, IDENT_TEMPLATE, x);

    if (x % 2 == 0 && skip_even) continue;

    bool put = kmin_put(kmin, index, (u32)(kmin_size_ - x - 1), strings[index]);
    ASSERT_TRUE(put);
  }

  ASSERT_EQ(kmin_size(kmin), kmin_size_);
  ASSERT_EQ(kmin_filled(kmin), (skip_even ? kmin_size_ / 2 : kmin_size_));

  ASSERT_TRUE(kmin_compact(kmin));

  ASSERT_EQ(kmin_size(kmin), (skip_even ? kmin_size_ / 2 : kmin_size_));
  ASSERT_EQ(kmin_filled(kmin), (skip_even ? kmin_size_ / 2 : kmin_size_));

  kmin_invert(kmin, invert);

  size_t result_count = kmin_find(kmin, 0, kmin_count);

  ASSERT_EQ(result_count, kmin_count);

  size_t check_count = skip_even ? kmin_count * 2 : kmin_count;

  for (size_t xx = 0; xx < check_count; xx++) {

    if (xx % 2 == 0 && skip_even) continue;

    size_t x = skip_even ? xx / 2 : xx;
    size_t score_x = skip_even ? xx - 1 : xx;

    fprintf(stderr,
            "results[%d].score = %d, results[%d].ident = %s\n",
            x,
            kmin_score_at(kmin, x),
            x,
            kmin_ident_at(kmin, x));

    char expected_ident_str[STR_SIZE] = {0};

    size_t expected_ident = invert ? score_x : kmin_size_ - score_x - 1;
    u32 expected_score = (u32)(invert ? kmin_size_ - 1 - score_x : score_x);

    snprintf(expected_ident_str, sizeof(expected_ident_str), IDENT_TEMPLATE, expected_ident);

    ASSERT_STREQ(expected_ident_str, kmin_ident_at(kmin, x));
    ASSERT_EQ(expected_score, kmin_score_at(kmin, x));
  }

  kmin_destroy(&kmin);
  ASSERT_EQ(kmin, nullptr);

  for (size_t x = 0; x < kmin_size_; x++) {
    free(strings[x]);
  }

  free(strings);
}

TEST_F(LibpiksiTests, kmin_small)
{
  do_kmin_test(KMIN_SMALL_SIZE, KMIN_COUNT, false, false);
}

TEST_F(LibpiksiTests, kmin_small_compact)
{
  do_kmin_test(KMIN_SMALL_SIZE, KMIN_COUNT, false, true);
}

TEST_F(LibpiksiTests, kmin_medium)
{
  do_kmin_test(KMIN_MEDIUM_SIZE, KMIN_COUNT, false, false);
}

TEST_F(LibpiksiTests, kmin_medium_invert)
{
  do_kmin_test(KMIN_MEDIUM_SIZE, KMIN_COUNT, true, false);
}

TEST_F(LibpiksiTests, kmin_large_macro_timsort)
{
  SKIP_LONG_TEST();
  do_kmin_test(KMIN_LARGE_SIZE, KMIN_COUNT, false, false);
}
