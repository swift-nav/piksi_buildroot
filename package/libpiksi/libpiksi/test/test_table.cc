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

#include <libpiksi/table.h>

#include <libpiksi_tests.h>

typedef struct entry_s {
  int foo;
  int bar;
} entry_t;

void cleanup_entry(table_t *table, void **entry)
{
  (void)table;

  free(*entry);
  *entry = NULL;
}

TEST_F(LibpiksiTests, table)
{
  table_t *table = table_create(16, cleanup_entry);
  ASSERT_NE(table, nullptr);

  bool success = false;
  void *entry = nullptr;

  entry = table_get(table, "Not There");
  ASSERT_EQ(entry, nullptr);

  entry_t *the_entry = (entry_t *)calloc(1, sizeof(entry_t));
  *the_entry = (entry_t){.foo = 42, .bar = 2600};

  success = table_put(table, "Entry Key", the_entry);
  ASSERT_TRUE(success);

  entry_t *found_entry = (entry_t *)table_get(table, "Entry Key");

  ASSERT_NE(found_entry, nullptr);

  ASSERT_EQ(found_entry->foo, 42);
  ASSERT_EQ(found_entry->bar, 2600);

  table_destroy(&table);
  ASSERT_EQ(table, nullptr);
}
