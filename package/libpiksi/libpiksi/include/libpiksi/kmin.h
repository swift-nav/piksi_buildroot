/*
 * Copyright (C) 2019 Swift Navigation Inc.
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
 * @file    kmin.h
 * @brief   kmin algorithm
 *
 * @defgroup    kmin
 * @addtogroup  kmin
 * @{
 */

#ifndef LIBPIKSI_KMIN_H
#define LIBPIKSI_KMIN_H

#include <libpiksi/common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  KMIN_ALGORITHM_STDLIB_QSORT,
  KMIN_ALGORITHM_KLIB_INTROSORT,
  KMIN_ALGORITHM_MACRO_TIMSORT,
  KMIN_ALGORITHM_MACRO_QSORT,
} kmin_algorithm_t;

typedef struct kmin_element_s {
  bool is_filled;
  u32 score;
  const char *ident;
} kmin_element_t;

typedef struct kmin_s kmin_t;

kmin_t *kmin_create(size_t elements);

kmin_t *kmin_create_ex(size_t elements, kmin_algorithm_t algo);

void kmin_destroy(kmin_t **kmin);

size_t kmin_size(kmin_t *kmin);

size_t kmin_filled(kmin_t *kmin);

bool kmin_compact(kmin_t *kmin);

void kmin_invert(kmin_t *kmin, bool invert);

bool kmin_put(kmin_t *kmin, size_t index, u32 score, const char *ident);

ssize_t kmin_find(kmin_t *kmin, size_t kstart, size_t count, kmin_element_t *result_array);

#ifdef __cplusplus
}
#endif

#endif
