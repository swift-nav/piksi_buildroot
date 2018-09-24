/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef LIBPIKSI_METRICS_FOREACH_H
#define LIBPIKSI_METRICS_FOREACH_H
// clang-format off

// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***
// *** WARNING: AVERT YOUR EYES... DO NOT LOOK FURTHER ***

#define  _M_FOREACH_1(Context, TheMacro, x)        TheMacro(Context, x)
#define  _M_FOREACH_2(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_1(Context, TheMacro, __VA_ARGS__)
#define  _M_FOREACH_3(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_2(Context, TheMacro, __VA_ARGS__)
#define  _M_FOREACH_4(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_3(Context, TheMacro, __VA_ARGS__)
#define  _M_FOREACH_5(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_4(Context, TheMacro, __VA_ARGS__)
#define  _M_FOREACH_6(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_5(Context, TheMacro, __VA_ARGS__)
#define  _M_FOREACH_7(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_6(Context, TheMacro, __VA_ARGS__)
#define  _M_FOREACH_8(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_7(Context, TheMacro, __VA_ARGS__)
#define  _M_FOREACH_9(Context, TheMacro, x, ...)   TheMacro(Context, x)  _M_FOREACH_8(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_10(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_9(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_11(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_10(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_12(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_11(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_13(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_12(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_14(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_13(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_15(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_14(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_16(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_15(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_17(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_16(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_18(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_17(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_19(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_18(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_20(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_19(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_21(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_20(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_22(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_21(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_23(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_22(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_24(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_23(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_25(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_24(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_26(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_25(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_27(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_26(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_28(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_27(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_29(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_28(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH_30(Context, TheMacro, x, ...)   TheMacro(Context, x) _M_FOREACH_29(Context, TheMacro, __VA_ARGS__)

#define _M_FOREACH_NARG(...) _M_FOREACH_NARG_(__VA_ARGS__, _M_FOREACH_RSEQ_N())
#define _M_FOREACH_NARG_(...) _M_FOREACH_ARG_N(__VA_ARGS__)
#define _M_FOREACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, N, ...) N
#define _M_FOREACH_RSEQ_N() 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#define _M_CONCAT(arg1, arg2)   _M_CONCAT1(arg1, arg2)
#define _M_CONCAT1(arg1, arg2)  _M_CONCAT2(arg1, arg2)
#define _M_CONCAT2(arg1, arg2)  arg1##arg2

#define _M_FOREACH_(N, Context, TheMacro, ...) _M_CONCAT(_M_FOREACH_, N)(Context, TheMacro, __VA_ARGS__)
#define _M_FOREACH(Context, TheMacro, ...) _M_FOREACH_(_M_FOREACH_NARG(__VA_ARGS__), Context, TheMacro, __VA_ARGS__)

// clang-format on
#endif
