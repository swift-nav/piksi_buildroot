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

typedef struct kmin_element_s {
  bool is_filled;
  u32 score;
  const char *ident;
} kmin_element_t;

struct kmin_s {
  kmin_element_t *arr;
  size_t size;
  size_t filled;
  bool invert;
  kmin_element_t *result;
  size_t result_count;
};

#define KMIN_COMPARE(l, r) ((l).score == (r).score ? 0 : ((l).score < (r).score ? -1 : 1))

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
  ctx->filled = 0;
  ctx->result = NULL;
  ctx->result_count = 0;

  return ctx;
}

void kmin_destroy(kmin_t **pctx)
{
  if (pctx == NULL || *pctx == NULL) return;

  if ((*pctx)->result != NULL) {
    free((*pctx)->result);
    (*pctx)->result = NULL;
  }

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
  if (kmin->result != NULL) {

    free(kmin->result);

    kmin->result = NULL;
    kmin->result_count = 0;
  }

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

  return true;
}

static void handle_macro_timsort(kmin_t *kmin)
{
  if (kmin->invert) {
    _macro_kmin_inv_tim_sort(kmin->arr, kmin->size);
  } else {
    _macro_kmin_tim_sort(kmin->arr, kmin->size);
  }
}

ssize_t kmin_find(kmin_t *kmin, size_t kstart, size_t count)
{
  if (kmin->filled != kmin->size) {
    PK_LOG_ANNO(LOG_ERR, "fill size must equal allocated size");
    return -1;
  }

  if (kstart >= kmin->size) {
    PK_LOG_ANNO(LOG_ERR, "kstart parameter cannot be larger than size");
    return -1;
  }

  if (kmin->result != NULL) {
    free(kmin->result);
    kmin->result = NULL;
  }

  count = SWFT_MIN(kmin->size, count);
  kmin->result = calloc(count, sizeof(kmin_element_t));

  if (kmin->result == NULL) {
    PK_LOG_ANNO(LOG_ERR, "calloc failed");
    return -1;
  }

  kmin->result_count = count;
  handle_macro_timsort(kmin);

  for (size_t k = kstart; k < count; k++) {
    kmin->result[k] = kmin->arr[k];
  }

  return sizet_to_ssizet(count);
}

ssize_t kmin_result_count(kmin_t *kmin)
{
  if (kmin->result == NULL) {

    PK_LOG_ANNO(LOG_ERR, "no results ready");
    return -1;
  }

  return sizet_to_ssizet(kmin->result_count);
}

bool kmin_result_at(kmin_t *kmin, size_t index, u32 *score_out, const char **ident_out)
{
  if (kmin->result == NULL) {
    PK_LOG_ANNO(LOG_ERR, "no results ready");
    return false;
  }

  if (index >= kmin->result_count) {
    PK_LOG_ANNO(LOG_ERR, "requested index too large");
    return false;
  }

  if (score_out != NULL) {
    *score_out = kmin->result[index].score;
  }

  if (ident_out != NULL) {
    *ident_out = kmin->result[index].ident;
  }

  return true;
}

u32 kmin_score_at(kmin_t *kmin, size_t index)
{
  u32 score = 0;

  bool result_at = kmin_result_at(kmin, index, &score, NULL);
  assert(result_at);

  return score;
}

const char *kmin_ident_at(kmin_t *kmin, size_t index)
{
  const char *ident = NULL;

  bool result_at = kmin_result_at(kmin, index, NULL, &ident);
  assert(result_at);

  return ident;
}
