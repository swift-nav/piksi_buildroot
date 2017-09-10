/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jason Mobarak <jason@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_DEBUG_MACROS_H
#define SWIFTNAV_DEBUG_MACROS_H

#include <sys/syslog.h>

#  define _WT_WARN(S, M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  if (S->_log_msg) S->_log_msg(LOG_WARNING, _Msg1); }

#  define _WT_DEBUG(S, M, ...) { \
  char _Msg1[1024]; snprintf(_Msg1, sizeof(_Msg1), M, ##__VA_ARGS__); \
  if (S->_log_msg) S->_log_msg(LOG_DEBUG, _Msg1); }

#ifndef TEST_WRITE_THREAD
#  define _WT_DEBUG_TEST(M, ...)
#else
#  define _WT_DEBUG_TEST(S, M, ...) _WT_DEBUG(S, M, ##__VA_ARGS__)
#endif

#endif//SWIFTNAV_DEBUG_MACROS_H
