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
 *
 * @brief   Find the K minimum (or maximum) elements of a group
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

/**
 * @brief Opaque context for kmin object
 */
typedef struct kmin_s kmin_t;

/**
 * @brief Create a kmin object
 *
 * @param[in] elements   The number of elements this set will cover
 *
 * @return               Pointer to the created context, or NULL on failure
 */
kmin_t *kmin_create(size_t elements);

/**
 * @brief Destory a kmin object
 *
 * @param[inout] kmin    The object being free'd, NULL'd on completion
 */
void kmin_destroy(kmin_t **kmin);

/**
 * @brief The max size of the set being covered
 */
size_t kmin_size(kmin_t *kmin);

/**
 * @brief How many elements of the current set have been filled.
 */
size_t kmin_filled(kmin_t *kmin);

/**
 * @brief Compact the set so that the filled elements match the total size.
 *
 * @details This is useful if you know the size of an input set but what to
 *          exclude some elements from the final set.
 *
 * @return  True if the compaction succeeded.  Only fails if memory allocation fails.
 */
bool kmin_compact(kmin_t *kmin);

/**
 * @brief Invert the sort and return maximums instead of minimums.
 */
void kmin_invert(kmin_t *kmin, bool invert);

/**
 * @brief Insert an element into the set.
 *
 * @param[in] kmin    The kmin object
 * @param[in] index   The index of the item in the set
 * @param[in] score   The score (ranking) for this item
 * @param[in] ident   A string identity describing the item
 *
 * @return    False if index is outside the bounds of the set, or if
 *            memory allocation fails.
 */
bool kmin_put(kmin_t *kmin, size_t index, u32 score, const char *ident);

/**
 * @brief Find the K smallest (or largest) elements of the set.
 *
 * @param[in] kmin    The kmin object
 * @param[in] kstart  The Kth element to start at
 * @param[in] count   The number of elements from the Kth element
 *
 * @return    The number of elements in the result set, -1 on failure.
 */
ssize_t kmin_find(kmin_t *kmin, size_t kstart, size_t count);

/**
 * @brief The number of elements in the result set
 *
 * @return    -1 if the result set has not been computed yet, or the size
 *            of the result set on success.
 */
ssize_t kmin_result_count(kmin_t *kmin);

/**
 * @brief Fetch a particular element from a result set.
 *
 * @param[in]  kmin    The kmin object
 * @param[out] index   The requested index
 * @param[out] score   The output score
 * @param[out] ident   The output ident
 *
 * @return    False if the result set has not been computed yet or if
 *            the request index is outside the bounds of the result set.
 */
bool kmin_result_at(kmin_t *kmin, size_t index, u32 *score, const char **ident);

/**
 * @brief Fetch a score element from a result set.
 *
 * @param[in]  kmin    The kmin object
 * @param[out] index   The requested index
 *
 * @return    The request score, asserts internally if an invalid index
 *            is requested, or if the result set hasn't been computed yet.
 */
u32 kmin_score_at(kmin_t *kmin, size_t index);

/**
 * @brief Fetch a ident element from a result set.
 *
 * @param[in]  kmin    The kmin object
 * @param[out] index   The requested index
 *
 * @return    The request ident, asserts internally if an invalid index
 *            is requested, or if the result set hasn't been computed yet.
 */
const char *kmin_ident_at(kmin_t *kmin, size_t index);

#ifdef __cplusplus
}
#endif

#endif

/** @} */
