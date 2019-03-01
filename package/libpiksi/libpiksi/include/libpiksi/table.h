/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file    table.h
 * @brief   Table API
 *
 * @defgroup    table Table
 * @addtogroup  table
 * @{
 */

#ifndef LIBPIKSI_TABLE_H
#define LIBPIKSI_TABLE_H

#include <libpiksi/util.h>
#include <libpiksi/common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct table_s;

typedef struct table_s table_t;

typedef void (*table_destroy_entry_fn_t)(void *entry);

table_t *table_create(size_t max_size);
table_t *table_create_ex(size_t max_size, table_destroy_entry_fn_t destroy_entry_fn);

void table_destroy(table_t **table);

bool table_put(table_t *table, const char *key, void *data);
void *table_get(table_t *table, const char *key);

NESTED_FN_TYPEDEF(bool,
                  table_foreach_fn_t,
                  table_t *table,
                  const char *key,
                  size_t index,
                  void *context);
void table_foreach_key(table_t *table, void *context, table_foreach_fn_t foreach_fn);

size_t table_count(table_t *table);

#ifdef __cplusplus
}
#endif

#define MAKE_TABLE_WRAPPER(Prefix, TDataTy)                                                        \
  inline __attribute__(                                                                            \
    (always_inline)) bool Prefix##_table_put(table_t *t, const char *key, TDataTy *d) /* NOLINT */ \
  {                                                                                                \
    return table_put(t, key, (void *)d);                                                           \
  }                                                                                                \
  inline __attribute__((always_inline))                                                            \
    TDataTy *Prefix##_table_get(table_t *t, const char *key) /*NOLINT*/                            \
  {                                                                                                \
    return (TDataTy *)table_get(t, key);                                                           \
  }

#endif /* LIBPIKSI_TABLE_H */

/** @} */
