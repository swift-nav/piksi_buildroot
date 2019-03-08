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

#include <stdlib.h>

#include <libpiksi/kmin.h>

#include <libpiksi/cast_check.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>

struct kmin_s {
  kmin_element_t *arr;
  size_t size;
  size_t filled;
  bool invert;
  kmin_algorithm_t algo;
  size_t largest_filled;
};

#define KMIN_COMPARE(l, r) ((l).score == (r).score ? 0 : ((l).score < (r).score ? -1 : 1))

static int qsort_compare(const void *pl, const void *pr)
{
  const kmin_element_t *l = pl;
  const kmin_element_t *r = pr;

  return KMIN_COMPARE(*l, *r);
}

static int qsort_compare_inv(const void *pl, const void *pr)
{
  return -1 * qsort_compare(pl, pr);
}

static bool compare_elements(kmin_element_t l, kmin_element_t r)
{
  return l.score < r.score;
}

static bool compare_elements_inv(kmin_element_t l, kmin_element_t r)
{
  return !compare_elements(l, r);
}

#pragma GCC diagnostic push

#include "vendor/ksort.h"

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"

KSORT_INIT(_kmin, kmin_element_t, compare_elements);
KSORT_INIT(_kmin_inv, kmin_element_t, compare_elements_inv);

#pragma GCC diagnostic pop

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wparentheses"

#define SORT_NAME _macro_kmin
#define SORT_TYPE kmin_element_t
#define SORT_CMP(x, y) KMIN_COMPARE(x, y)

#include "vendor/swenson_sort.h"

#undef SORT_NAME
#undef SORT_TYPE
#undef SORT_CMP

#define SORT_NAME _macro_kmin_inv
#define SORT_TYPE kmin_element_t
#define SORT_CMP(x, y) (-1 * KMIN_COMPARE(x, y))

#include "vendor/swenson_sort.h"

#pragma GCC diagnostic pop

kmin_t *kmin_create(size_t size)
{
  return kmin_create_ex(size, KMIN_ALGORITHM_MACRO_TIMSORT);
}

kmin_t *kmin_create_ex(size_t size, kmin_algorithm_t algo)
{
  kmin_t *ctx = calloc(1, sizeof(kmin_t));

  if (ctx == NULL) {
    PK_LOG_ANNO(LOG_ERR, "calloc failed");
    return NULL;
  }

  ctx->arr = calloc(size, sizeof(kmin_element_t));

  if (ctx->arr == NULL) {
    PK_LOG_ANNO(LOG_ERR, "calloc failed");
    return NULL;
  }

  ctx->size = size;
  ctx->algo = algo;

  ctx->largest_filled = 0;
  ctx->filled = 0;

  return ctx;
}

void kmin_destroy(kmin_t **pctx)
{
  if (pctx == NULL || *pctx == NULL) return;

  free((*pctx)->arr);
  (*pctx)->arr = NULL;

  free(*pctx);
  *pctx = NULL;
}

size_t kmin_size(kmin_t *kmin)
{
  return kmin->size;
}

size_t kmin_filled(kmin_t *kmin)
{
  return kmin->filled;
}

bool kmin_compact(kmin_t *kmin)
{
  if (kmin->filled == kmin->size) {
    return true;
  }

  kmin_element_t *new_arr = calloc(kmin->filled, sizeof(kmin_element_t));

  if (new_arr == NULL) {
    PK_LOG_ANNO(LOG_ERR, "calloc failed");
    return false;
  }

  size_t new_arr_idx = 0;

  for (size_t idx = 0; idx < kmin->size; idx++) {
    if (!kmin->arr[idx].is_filled) continue;
    new_arr[new_arr_idx++] = kmin->arr[idx];
  }

  free(kmin->arr);
  kmin->arr = new_arr;

  kmin->size = kmin->filled;

  return true;
}

void kmin_invert(kmin_t *kmin, bool invert)
{
  kmin->invert = invert;
}

bool kmin_put(kmin_t *kmin, size_t index, u32 score, const char *ident)
{
  if (index >= kmin->size) {
    return false;
  }

  if (kmin->arr[index].ident != NULL) {
    return false;
  }

  kmin->arr[index].score = score;
  kmin->arr[index].ident = ident;
  kmin->arr[index].is_filled = true;

  kmin->filled++;
  kmin->largest_filled = SWFT_MAX(index, kmin->largest_filled);

  return true;
}

static void handle_stdlib_qsort(kmin_t *kmin)
{
  if (kmin->invert) {
    qsort(kmin->arr, kmin->size, sizeof(kmin_element_t), qsort_compare_inv);
  } else {
    qsort(kmin->arr, kmin->size, sizeof(kmin_element_t), qsort_compare);
  }
}

static void handle_klib_introsort(kmin_t *kmin)
{
  if (kmin->invert) {
    ks_introsort(_kmin_inv, kmin->size, kmin->arr);
  } else {
    ks_introsort(_kmin, kmin->size, kmin->arr);
  }
}

static void handle_macro_qsort(kmin_t *kmin)
{
  if (kmin->invert) {
    _macro_kmin_inv_quick_sort(kmin->arr, kmin->size);
  } else {
    _macro_kmin_quick_sort(kmin->arr, kmin->size);
  }
}

static void handle_macro_timsort(kmin_t *kmin)
{
  if (kmin->invert) {
    _macro_kmin_inv_tim_sort(kmin->arr, kmin->size);
  } else {
    _macro_kmin_tim_sort(kmin->arr, kmin->size);
  }
}

ssize_t kmin_find(kmin_t *kmin, size_t kstart, size_t count, kmin_element_t *results)
{
  if (kmin->filled != kmin->size) {
    PK_LOG_ANNO(LOG_ERR, "fill size must equal allocated size");
    return -1;
  }

  if (kstart >= kmin->size) {
    PK_LOG_ANNO(LOG_ERR, "kstart parameter cannot be larger than size");
    return -1;
  }

  count = SWFT_MIN(kmin->size, count);

  switch (kmin->algo) {
  case KMIN_ALGORITHM_STDLIB_QSORT: handle_stdlib_qsort(kmin); break;
  case KMIN_ALGORITHM_KLIB_INTROSORT: handle_klib_introsort(kmin); break;
  case KMIN_ALGORITHM_MACRO_QSORT: handle_macro_qsort(kmin); break;
  case KMIN_ALGORITHM_MACRO_TIMSORT: handle_macro_timsort(kmin); break;
  default: assert(false);
  }

  for (size_t k = kstart; k < count; k++) {
    results[k] = kmin->arr[k];
  }

  return sizet_to_ssizet(count);
}
