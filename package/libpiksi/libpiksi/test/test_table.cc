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

typedef struct foreach_s {
  bool saw_key1;
  bool saw_key2;
} foreach_t;

bool foreach_key(table_t *table, const char *key, size_t index, void *context)
{
  foreach_t *foreach = (foreach_t *)context;

  EXPECT_NE(table, nullptr);
  if (strcmp(key, "Entry Key") == 0) {
    foreach
      ->saw_key1 = true;
  } else if (strcmp(key, "Entry Key 2") == 0) {
    foreach
      ->saw_key2 = true;
  } else {
    bool found_unexpected_key = true;
    EXPECT_FALSE(found_unexpected_key);
  }
  return true;
}

TEST_F(LibpiksiTests, table)
{
  table_t *table = table_create(16, cleanup_entry);
  ASSERT_NE(table, nullptr);

  bool success = false;
  void *entry = nullptr;

  entry = table_get(table, "Not There");
  ASSERT_EQ(entry, nullptr);

  {
    entry_t *the_entry = (entry_t *)calloc(1, sizeof(entry_t));
    *the_entry = (entry_t){.foo = 42, .bar = 2600};

    success = table_put(table, "Entry Key", the_entry);
    ASSERT_TRUE(success);

    entry_t *found_entry = (entry_t *)table_get(table, "Entry Key");

    ASSERT_NE(found_entry, nullptr);

    ASSERT_EQ(found_entry->foo, 42);
    ASSERT_EQ(found_entry->bar, 2600);
  }
  {
    entry_t *the_entry = (entry_t *)calloc(1, sizeof(entry_t));
    *the_entry = (entry_t){.foo = 4242, .bar = 26000};

    success = table_put(table, "Entry Key 2", the_entry);
    ASSERT_TRUE(success);

    entry_t *found_entry = (entry_t *)table_get(table, "Entry Key 2");

    ASSERT_NE(found_entry, nullptr);

    ASSERT_EQ(found_entry->foo, 4242);
    ASSERT_EQ(found_entry->bar, 26000);
  }
  {
    entry_t *found_entry = (entry_t *)table_get(table, "Entry Key");

    ASSERT_NE(found_entry, nullptr);

    ASSERT_EQ(found_entry->foo, 42);
    ASSERT_EQ(found_entry->bar, 2600);
  }
  {
    foreach_t foreach = {.saw_key1 = false, .saw_key2 = false};
    table_foreach_key(table, &foreach, foreach_key);

    ASSERT_TRUE(foreach.saw_key1);
    ASSERT_TRUE(foreach.saw_key2);
  }
  {
    ASSERT_EQ(table_count(table), 2);
  }

  table_destroy(&table);
  ASSERT_EQ(table, nullptr);
}
