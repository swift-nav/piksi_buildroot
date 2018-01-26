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

#ifndef __SWIFTNAV_ZMQ_ADAPTER_LOGGING_H
#define __SWIFTNAV_ZMQ_ADAPTER_LOGGING_H

#include <stdarg.h>
#include <syslog.h>

#define LOG_SBP LOG_LOCAL1

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*zmq_adapter_log_fn_t) (int priority, const char *format, va_list args);

void zmq_adapter_log(int priority, const char* format, ...);
void zmq_adapter_set_log_fn(zmq_adapter_log_fn_t log_fn);

#ifdef __cplusplus
} // extern "C"
#endif

#endif//__SWIFTNAV_ZMQ_ADAPTER_LOGGING_H
