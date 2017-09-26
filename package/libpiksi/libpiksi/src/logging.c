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

static char log_ident[256];

int logging_init(const char *identity)
{
  snprintf(log_ident, sizeof(log_ident), "%s", identity);
  openlog(identity, OPTIONS, FACILITY);
  return 0;
}

void logging_deinit(void)
{
  snprintf(log_ident, sizeof(log_ident), "");
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

#define NUM_LOG_LEVELS 8

void sbp_log(int priority, const char *msg_text)
{
  const char *log_args[NUM_LOG_LEVELS] = {"emerg", "alert", "crit",
                                          "error", "warn", "notice",
                                          "info", "debug"};

  if (priority < 0 || priority >= NUM_LOG_LEVELS) {
    priority = LOG_INFO;
  }

  FILE *output;
  char cmd_buf[256];
  snprintf(cmd_buf, sizeof(cmd_buf), "sbp_log --%s", log_args[priority]);
  output = popen (cmd_buf, "w");

  if (output == 0) {
    piksi_log(LOG_ERR, "couldn't call sbp_log.");
    return;
  }

  char msg_buf[1024];
  snprintf(msg_buf, sizeof(msg_buf), "%s: %s", log_ident, msg_text);

  fputs(msg_buf, output);

  if (ferror (output) != 0) {
    piksi_log(LOG_ERR, "output to sbp_log failed.");
  }

  if (pclose (output) != 0) {
    piksi_log(LOG_ERR, "couldn't close sbp_log call.");
    return;
  }
}
