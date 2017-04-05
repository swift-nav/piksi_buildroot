/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/logging.h>
#include <syslog.h>

#define FACILITY LOG_LOCAL0
#define OPTIONS (LOG_CONS | LOG_PID | LOG_NDELAY)

int logging_init(const char *identity)
{
  openlog(identity, OPTIONS, FACILITY);
  return 0;
}

void logging_deinit(void)
{
  closelog();
}

void piksi_log(int priority, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vsyslog(priority, format, ap);
  va_end(ap);
}

void piksi_vlog(int priority, const char *format, va_list ap)
{
  vsyslog(priority, format, ap);
}
