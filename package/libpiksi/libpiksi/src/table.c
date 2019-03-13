/*
 * Copyright (C) 2018-2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/table.h>

#include "vendor/uthash.h"

typedef struct hash_entry_s {
  char *key;
  void *data;
  UT_hash_handle hh;
} hash_entry;

typedef struct table_s {
  table_destroy_entry_fn_t destroy_entry;
  hash_entry *hash_entries;
  size_t max_entries;
} table_t;

table_t *table_create(size_t max_entries)
{
  return table_create_ex(max_entries, NULL);
}

table_t *table_create_ex(size_t max_entries, table_destroy_entry_fn_t destroy_entry_fn)
{
  table_t *table = calloc(1, sizeof(*table));

  table->destroy_entry = destroy_entry_fn;
  table->max_entries = max_entries;

  return table;
}

void table_destroy(table_t **ptable)
{
  if (ptable == NULL || *ptable == NULL) return;

  table_t *table = *ptable;
  hash_entry *entry, *tmp;

  HASH_ITER(hh, table->hash_entries, entry, tmp)
  {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    HASH_DEL(table->hash_entries, entry);
#pragma GCC diagnostic pop
    if (table->destroy_entry != NULL) {
      table->destroy_entry(entry->data);
    } else {
      free(entry->data);
    }
    free(entry->key);
    free(entry);
  }

  free(table);
  *ptable = NULL;
}

bool table_put(table_t *table, const char *key, void *data)
{
  hash_entry *entry = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
  HASH_FIND_STR(table->hash_entries, key, entry);
#pragma GCC diagnostic pop

  if (entry != NULL) {
    PK_LOG_ANNO(LOG_ERR, "overwrite is not supported");
    return false;
  }

  if (table_count(table) >= table->max_entries) {
    PK_LOG_ANNO(LOG_ERR, "table entries exhausted");
    return false;
  }

  hash_entry *new_entry = calloc(1, sizeof(*new_entry));

  new_entry->key = strdup(key);
  new_entry->data = data;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
  HASH_ADD_KEYPTR(hh, table->hash_entries, new_entry->key, strlen(new_entry->key), new_entry);
#pragma GCC diagnostic pop

  return true;
}

void *table_get(table_t *table, const char *key_in)
{
  hash_entry *entry = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
  HASH_FIND_STR(table->hash_entries, key_in, entry);
#pragma GCC diagnostic push

  return entry != NULL ? entry->data : NULL;
}

void table_foreach_key(table_t *table, void *context, table_foreach_fn_t func)
{
  size_t index = 0;
  hash_entry *entry, *tmp;
  HASH_ITER(hh, table->hash_entries, entry, tmp)
  {
    if (!func(table, entry->key, index++, context)) break;
  }
}

size_t table_count(table_t *table)
{
  return HASH_COUNT(table->hash_entries);
}
