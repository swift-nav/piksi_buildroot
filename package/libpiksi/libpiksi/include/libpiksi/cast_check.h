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

#ifndef __SWIFTNAV_CAST_CHECK_H
#define __SWIFTNAV_CAST_CHECK_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef unsigned long unsigned_long;

#define _CHECK_CAST_SIGNED(var, FromType, ToType) \
  assert((var < 0) ? ((INVALID_CAST_FROM_##FromType##_TO_##ToType)) : true)

#define _CHECK_CAST_UNSIGNED(var, FromType, ToType, Max) \
  assert((var > (FromType)Max) ? ((INVALID_CAST_FROM_##FromType##_TO_##ToType)) : true)

#define _CHECK_CAST_UNSIGNED_MIN(var, FromType, ToType, Min) \
  assert((var < 0 && var < (FromType)Min) ? ((INVALID_CAST_FROM_##FromType##_TO_##ToType)) : true)

#define _DEFINE_HELPER_VAR(FromType, ToType) \
  static const bool INVALID_CAST_FROM_##FromType##_TO_##ToType __attribute__((unused)) = false;

/* size_t -> ssize_t */

#define CHECK_SIZET_TO_SSIZET(var) _CHECK_CAST_UNSIGNED(var, size_t, ssize_t, SSIZE_MAX)

_DEFINE_HELPER_VAR(size_t, ssize_t);

#define SIZET_TO_SSIZET(var) ((ssize_t)(var))

#define sizet_to_ssizet(s) (CHECK_SIZET_TO_SSIZET(s), SIZET_TO_SSIZET(s))

/* size_t -> int */

#define CHECK_SIZET_TO_INT(var) _CHECK_CAST_UNSIGNED(var, size_t, int, INT_MAX)

_DEFINE_HELPER_VAR(size_t, int);

#define SIZET_TO_INT(var) ((int)(var))

#define sizet_to_int(s) (CHECK_SIZET_TO_INT(s), SIZET_TO_INT(s))

/* ssize_t -> size_t */

#define CHECK_SSIZET_TO_SIZET(var) _CHECK_CAST_SIGNED(var, ssize_t, size_t)

_DEFINE_HELPER_VAR(ssize_t, size_t);

#define SSIZET_TO_SIZET(var) ((size_t)(var))

#define ssizet_to_sizet(s) (CHECK_SSIZET_TO_SIZET(s), SSIZET_TO_SIZET(s))

/* ssize_t -> int */

#define CHECK_SSIZET_TO_INT(var)                     \
  (_CHECK_CAST_UNSIGNED(var, ssize_t, int, INT_MAX), \
   _CHECK_CAST_UNSIGNED_MIN(var, ssize_t, int, INT_MIN))

_DEFINE_HELPER_VAR(ssize_t, int);

#define SSIZET_TO_INT(var) ((int)(var))

#define ssizet_to_int(s) (CHECK_SSIZET_TO_INT(s), SSIZET_TO_INT(s))

/* unsigned long -> uint16_t */

#define CHECK_ULONG_TO_UINT16(var) _CHECK_CAST_UNSIGNED(var, unsigned_long, uint16_t, UINT16_MAX)

_DEFINE_HELPER_VAR(unsigned_long, uint16_t);

#define ULONG_TO_UINT16(var) ((uint16_t)(var))

#define ulong_to_uint16(s) (CHECK_ULONG_TO_UINT16(s), ULONG_TO_UINT16(s))

/* unsigned long -> int */

#define CHECK_ULONG_TO_INT(var) _CHECK_CAST_UNSIGNED(var, unsigned_long, int, INT_MAX)

_DEFINE_HELPER_VAR(unsigned_long, int);

#define ULONG_TO_INT(var) ((int)(var))

#define ulong_to_int(s) (CHECK_ULONG_TO_INT(s), ULONG_TO_INT(s))

/* size_t -> uint32_t */

#define CHECK_SIZET_TO_UINT32(var) _CHECK_CAST_UNSIGNED(var, size_t, uint32_t, UINT32_MAX)

#define SIZET_TO_UINT32(var) ((uint32_t)(var))

_DEFINE_HELPER_VAR(size_t, uint32_t);

#define sizet_to_uint32(s) (CHECK_SIZET_TO_UINT32(s), SIZET_TO_UINT32(s))

/* int -> size_t */

#define CHECK_INT_TO_SIZET(var) _CHECK_CAST_SIGNED(var, int, size_t)

_DEFINE_HELPER_VAR(int, size_t);

#define INT_TO_SIZET(var) ((size_t)(var))

#define int_to_sizet(s) (CHECK_INT_TO_SIZET(s), INT_TO_SIZET(s))

/* size_t -> u8 */

#define CHECK_SIZET_TO_UINT8(var) _CHECK_CAST_UNSIGNED(var, size_t, uint8_t, UINT8_MAX)

#define SIZET_TO_UINT8(var) ((uint8_t)(var))

_DEFINE_HELPER_VAR(size_t, uint8_t);

#define sizet_to_uint8(s) (CHECK_SIZET_TO_UINT8(s), SIZET_TO_UINT8(s))

/* int -> uint8_t */

#define CHECK_INT_TO_UINT8(var) \
  (_CHECK_CAST_SIGNED(var, int, uint8_t), _CHECK_CAST_UNSIGNED(var, int, uint8_t, UINT8_MAX))

_DEFINE_HELPER_VAR(int, uint8_t);

#define INT_TO_UINT8(var) ((uint8_t)(var))

#define int_to_uint8(s) (CHECK_INT_TO_UINT8(s), INT_TO_UINT8(s))

/* int -> uint32_t */

#define CHECK_INT_TO_UINT32(var) _CHECK_CAST_SIGNED(var, int, uint32_t)

_DEFINE_HELPER_VAR(int, uint32_t);

#define INT_TO_UINT32(var) ((uint32_t)(var))

#define int_to_uint32(s) (CHECK_INT_TO_UINT32(s), INT_TO_UINT32(s))

/* uint32_t -> int32_t */

#define CHECK_UINT32_TO_INT32(var) _CHECK_CAST_UNSIGNED(var, uint32_t, int32_t, INT32_MAX)

_DEFINE_HELPER_VAR(uint32_t, int32_t);

#define UINT32_TO_INT32(var) ((int32_t)(var))

#define uint32_to_int32(s) (CHECK_UINT32_TO_INT32(s), UINT32_TO_INT32(s))

#endif
