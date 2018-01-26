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

#include "logging.h"
#include <syslog.h>

#include <libpiksi/logging.h>

zmq_adapter_log_fn_t override_log_fn = NULL;

void zmq_adapter_log(int priority, const char* format, ...)
{
  va_list ap;
  va_start(ap, format);
  if (override_log_fn == NULL) {
    if ((priority & LOG_FACMASK) == LOG_SBP) {
      sbp_vlog(priority, format, ap);
      priority &= ~LOG_FACMASK;
    }
    piksi_vlog(priority, format, ap);
  } else {
    override_log_fn(priority, format, ap);
  }
  va_end(ap);
}

void zmq_adapter_set_log_fn(zmq_adapter_log_fn_t log_fn)
{
  override_log_fn = log_fn;
}
