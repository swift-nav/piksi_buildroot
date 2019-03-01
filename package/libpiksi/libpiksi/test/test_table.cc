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

#define SMALL_TABLE_MAX 64

typedef struct entry_s {
  int foo;
  int bar;
} entry_t;

static bool cleanup_called = false;

void cleanup_entry(void *entry)
{
  cleanup_called = true;
  free(entry);
}

typedef struct foreach_s {
  bool saw_key1;
  bool saw_key2;
} foreach_t;

bool foreach_key(table_t *table, const char *key, size_t index, void *context)
{
  (void)index;
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
  table_t *table = table_create_ex(SMALL_TABLE_MAX, cleanup_entry);
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

  ASSERT_TRUE(cleanup_called);
}

TEST_F(LibpiksiTests, default_free)
{
  table_t *table = table_create(SMALL_TABLE_MAX); /* defaults to just `free()` */
  ASSERT_NE(table, nullptr);

  bool success = false;
  void *entry = nullptr;

  entry_t *the_entry = (entry_t *)calloc(1, sizeof(entry_t));
  *the_entry = (entry_t){.foo = 4004, .bar = 2606};

  success = table_put(table, "Entry Key", the_entry);
  ASSERT_TRUE(success);

  entry_t *found_entry = (entry_t *)table_get(table, "Entry Key");

  ASSERT_NE(found_entry, nullptr);

  ASSERT_EQ(found_entry->foo, 4004);
  ASSERT_EQ(found_entry->bar, 2606);

  table_destroy(&table);
  ASSERT_EQ(table, nullptr);
}

TEST_F(LibpiksiTests, large_table)
{
  const size_t TABLE_SIZE = 16 * 1024;

  table_t *table = table_create(TABLE_SIZE);
  ASSERT_NE(table, nullptr);

  bool success = false;
  void *entry = nullptr;

  entry = table_get(table, "Not There");
  ASSERT_EQ(entry, nullptr);

  for (size_t i = 0; i < TABLE_SIZE; i++) {

    entry_t *the_entry = (entry_t *)calloc(1, sizeof(entry_t));
    *the_entry = (entry_t){.foo = (int)i, .bar = (int)i * 2};

    char key_val[32];
    sprintf(key_val, "Entry Key #%08d", i);

    success = table_put(table, key_val, the_entry);
    ASSERT_TRUE(success);

    ASSERT_EQ(((entry_t *)table_get(table, key_val))->foo, i);
    ASSERT_EQ(((entry_t *)table_get(table, key_val))->bar, i * 2);
  }

  table_destroy(&table);
  ASSERT_EQ(table, nullptr);
}

TEST_F(LibpiksiTests, exhaust)
{
  table_t *table = table_create(SMALL_TABLE_MAX);
  ASSERT_NE(table, nullptr);

  bool success = false;
  void *entry = nullptr;

  entry = table_get(table, "Not There");
  ASSERT_EQ(entry, nullptr);

  size_t i = 0;
  for (; i < SMALL_TABLE_MAX; i++) {

    entry_t *the_entry = (entry_t *)calloc(1, sizeof(entry_t));
    *the_entry = (entry_t){.foo = (int)i, .bar = (int)i * 2};

    char key_val[32];
    sprintf(key_val, "Entry Key #%08d", i);

    success = table_put(table, key_val, the_entry);
    ASSERT_TRUE(success);

    ASSERT_EQ(((entry_t *)table_get(table, key_val))->foo, i);
    ASSERT_EQ(((entry_t *)table_get(table, key_val))->bar, i * 2);
  }

  entry_t *the_entry = (entry_t *)calloc(1, sizeof(entry_t));
  *the_entry = (entry_t){.foo = (int)i, .bar = (int)i * 2};

  char key_val[32];
  sprintf(key_val, "Entry Key #%08d", i);

  success = table_put(table, key_val, the_entry);
  ASSERT_FALSE(success);

  free(the_entry);

  table_destroy(&table);
  ASSERT_EQ(table, nullptr);
}
