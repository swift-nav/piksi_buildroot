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

#define _CC_UNLIKELY(x) __builtin_expect((x), 0)

#define _CC_CONCAT0(x, y) x##y

#define _CC_CONCAT(x, y) _CC_CONCAT0(x, y)

#define _CHECK_CAST_SIGNED(Value, FromType, ToType) \
  if (_CC_UNLIKELY(Value < 0)) assert(INVALID_CAST_FROM_##FromType##_TO_##ToType) /* NOLINT */

#define _CHECK_CAST_UNSIGNED(Value, FromType, ToType, Max) \
  if (_CC_UNLIKELY(Value > (FromType)Max))                 \
  assert(INVALID_CAST_FROM_##FromType##_TO_##ToType) /* NOLINT */

#define _CHECK_CAST_UNSIGNED_MIN(Value, FromType, ToType, Min)       \
  if (_CC_UNLIKELY(Value < 0 && Value < (FromType)Min)) /* NOLINT */ \
  assert(INVALID_CAST_FROM_##FromType##_TO_##ToType)    /* NOLINT */

#define _DEFINE_HELPER_VAR(FromType, ToType) \
  static const bool INVALID_CAST_FROM_##FromType##_TO_##ToType __attribute__((unused)) = false

/* size_t -> ssize_t */

#define CHECK_SIZET_TO_SSIZET(Value) _CHECK_CAST_UNSIGNED(Value, size_t, ssize_t, SSIZE_MAX)

_DEFINE_HELPER_VAR(size_t, ssize_t);

#define SIZET_TO_SSIZET(Value) ((ssize_t)(Value))

#define _sizet_to_ssizet(Expr, Var) \
  ({                                \
    size_t Var = Expr;              \
    CHECK_SIZET_TO_SSIZET(Var);     \
    SIZET_TO_SSIZET(Var);           \
  })

#define sizet_to_ssizet(Expr) _sizet_to_ssizet(Expr, _CC_CONCAT(r, __COUNTER__))

/* ssize_t -> size_t */

#define CHECK_SSIZET_TO_SIZET(Value) _CHECK_CAST_SIGNED(Value, ssize_t, size_t)

_DEFINE_HELPER_VAR(ssize_t, size_t);

#define SSIZET_TO_SIZET(Value) ((size_t)(Value))

#define _ssizet_to_sizet(Expr, Var) \
  ({                                \
    ssize_t Var = Expr;             \
    CHECK_SSIZET_TO_SIZET(Var);     \
    SSIZET_TO_SIZET(Var);           \
  })

#define ssizet_to_sizet(Expr) _ssizet_to_sizet(Expr, _CC_CONCAT(r, __COUNTER__))

/* ssize_t -> int */

_DEFINE_HELPER_VAR(ssize_t, int);

#define SSIZET_TO_INT(Value) ((int)(Value))

#define CHECK_SSIZET_TO_INT(Value)                          \
  ({                                                        \
    _CHECK_CAST_UNSIGNED(Value, ssize_t, int, INT_MAX);     \
    _CHECK_CAST_UNSIGNED_MIN(Value, ssize_t, int, INT_MIN); \
  })

#define _ssizet_to_int(Expr, Var) \
  ({                              \
    ssize_t Var = Expr;           \
    CHECK_SSIZET_TO_INT(Var);     \
    SSIZET_TO_INT(Var);           \
  })

#define ssizet_to_int(Expr) _ssizet_to_int(Expr, _CC_CONCAT(r, __COUNTER__))

/* unsigned long -> uint16_t */

#define CHECK_ULONG_TO_UINT16(Value) \
  _CHECK_CAST_UNSIGNED(Value, unsigned_long, uint16_t, UINT16_MAX)

_DEFINE_HELPER_VAR(unsigned_long, uint16_t);

#define ULONG_TO_UINT16(Value) ((uint16_t)(Value))

#define _ulong_to_uint16(Expr, Var) \
  ({                                \
    unsigned long Var = Expr;       \
    CHECK_ULONG_TO_UINT16(Var);     \
    ULONG_TO_UINT16(Var);           \
  })

#define ulong_to_uint16(Expr) _ulong_to_uint16(Expr, _CC_CONCAT(r, __COUNTER__))

/* unsigned long -> int */

#define CHECK_ULONG_TO_INT(Value) _CHECK_CAST_UNSIGNED(Value, unsigned_long, int, INT_MAX)

_DEFINE_HELPER_VAR(unsigned_long, int);

#define ULONG_TO_INT(Value) ((int)(Value))

#define _ulong_to_int(Expr, Var)                 \
  ({                                             \
    unsigned long Var = Expr;                    \
    CHECK_ULONG_TO_INT(Var); ULONG_TO_INT(Var)); \
  })

#define ulong_to_int(Expr) _ulong_to_int(Expr, _CC_CONCAT(r, __COUNTER__))

/* size_t -> uint32_t */

#define CHECK_SIZET_TO_UINT32(Value) _CHECK_CAST_UNSIGNED(Value, size_t, uint32_t, UINT32_MAX)

#define SIZET_TO_UINT32(Value) ((uint32_t)(Value))

_DEFINE_HELPER_VAR(size_t, uint32_t);

#define _sizet_to_uint32(Expr, Var) \
  ({                                \
    size_t Var = Expr;              \
    CHECK_SIZET_TO_UINT32(Var);     \
    SIZET_TO_UINT32(Var);           \
  })

#define sizet_to_uint32(Expr) _sizet_to_uint32(Expr, _CC_CONCAT(r, __COUNTER__))

/* int -> size_t */

#define CHECK_INT_TO_SIZET(Value) _CHECK_CAST_SIGNED(Value, int, size_t)

_DEFINE_HELPER_VAR(int, size_t);

#define INT_TO_SIZET(Value) ((size_t)(Value))

#define _int_to_sizet(Expr, Var) \
  ({                             \
    int Var = Expr;              \
    CHECK_INT_TO_SIZET(Var);     \
    INT_TO_SIZET(Var);           \
  })

#define int_to_sizet(Expr) _int_to_sizet(Expr, _CC_CONCAT(r, __COUNTER__))

/* size_t -> u8 */

#define CHECK_SIZET_TO_UINT8(Value) _CHECK_CAST_UNSIGNED(Value, size_t, uint8_t, UINT8_MAX)

#define SIZET_TO_UINT8(Value) ((uint8_t)(Value))

_DEFINE_HELPER_VAR(size_t, uint8_t);

#define _sizet_to_uint8(Expr, Var) \
  ({                               \
    size_t Var = Expr;             \
    CHECK_SIZET_TO_UINT8(Var);     \
    SIZET_TO_UINT8(Var);           \
  })

#define sizet_to_uint8(Expr) _sizet_to_uint8(Expr, _CC_CONCAT(r, __COUNTER__))

/* int -> uint8_t */

#define CHECK_INT_TO_UINT8(Value)                         \
  ({                                                      \
    _CHECK_CAST_SIGNED(Value, int, uint8_t);              \
    _CHECK_CAST_UNSIGNED(Value, int, uint8_t, UINT8_MAX); \
  })

_DEFINE_HELPER_VAR(int, uint8_t);

#define INT_TO_UINT8(Value) ((uint8_t)(Value))

#define _int_to_uint8(Expr, Var) \
  ({                             \
    int Var = Expr;              \
    CHECK_INT_TO_UINT8(Var);     \
    INT_TO_UINT8(Var);           \
  })

#define int_to_uint8(Expr) _int_to_uint8(Expr, _CC_CONCAT(r, __COUNTER__))

/* int -> uint32_t */

#define CHECK_INT_TO_UINT32(Value) _CHECK_CAST_SIGNED(Value, int, uint32_t)

_DEFINE_HELPER_VAR(int, uint32_t);

#define INT_TO_UINT32(Value) ((uint32_t)(Value))

#define _int_to_uint32(Expr, Var) \
  ({                              \
    int Var = Expr;               \
    CHECK_INT_TO_UINT32(Var);     \
    INT_TO_UINT32(Var);           \
  })

#define int_to_uint32(Expr) _int_to_uint32(Expr, _CC_CONCAT(r, __COUNTER__))

/* uint32_t -> int32_t */

#define CHECK_UINT32_TO_INT32(Value) _CHECK_CAST_UNSIGNED(Value, uint32_t, int32_t, INT32_MAX)

_DEFINE_HELPER_VAR(uint32_t, int32_t);

#define UINT32_TO_INT32(Value) ((int32_t)(Value))

#define _uint32_to_int32(Expr, Var) \
  ({                                \
    uint32_t Var = Expr;            \
    CHECK_UINT32_TO_INT32(Var);     \
    UINT32_TO_INT32(Var);           \
  })

#define uint32_to_int32(Expr) _uint32_to_int32(Expr, _CC_CONCAT(r, __COUNTER__))

/* size_t -> int */

_DEFINE_HELPER_VAR(size_t, int);

#define SIZET_TO_INT(Value) ((int)(Value))

#define CHECK_SIZET_TO_INT(Value) ({ _CHECK_CAST_UNSIGNED(Value, size_t, int, INT_MAX); })

#define _sizet_to_int(Expr, Var) \
  ({                             \
    size_t Var = Expr;           \
    CHECK_SIZET_TO_INT(Var);     \
    SIZET_TO_INT(Var);           \
  })

#define sizet_to_int(Expr) _sizet_to_int(Expr, _CC_CONCAT(r, __COUNTER__))

/* long -> size_t */

_DEFINE_HELPER_VAR(long, size_t);

#define LONG_TO_SIZET(Value) ((size_t)(Value))

#define CHECK_LONG_TO_SIZET(Value) ({ _CHECK_CAST_SIGNED(Value, long, size_t); })

#define _long_to_sizet(Expr, Var) \
  ({                              \
    long Var = Expr;              \
    CHECK_LONG_TO_SIZET(Var);     \
    LONG_TO_SIZET(Var);           \
  })

#define long_to_sizet(Expr) _long_to_sizet(Expr, _CC_CONCAT(r, __COUNTER__))

#endif
