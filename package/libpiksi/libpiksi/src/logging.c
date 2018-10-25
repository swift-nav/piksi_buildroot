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
#include <libpiksi/sbp_tx.h>
#include <libsbp/logging.h>
#include <stdarg.h>

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

#define SBP_TX_ENDPOINT "ipc:///var/run/sockets/external.sub"

#define FACILITY LOG_LOCAL0
#define OPTIONS (LOG_CONS | LOG_PID | LOG_NDELAY)

static bool log_stdout_only = false;

static sbp_tx_ctx_t *sbp_tx = NULL;

int logging_init(const char *identity)
{
  openlog(identity, OPTIONS, FACILITY);

  sbp_tx = sbp_tx_create(SBP_TX_ENDPOINT);
  if (NULL == s->sbp_tx) {
    piksi_log(LOG_ERR, "unable to initialize SBP tx endpoint.");
    return -1;
  }
  return 0;
}

void logging_log_to_stdout_only(bool enable)
{
  log_stdout_only = enable;
}

void logging_deinit(void)
{
  closelog();
  sbp_tx_destroy(&sbp_tx);
}

void piksi_log(int priority, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  piksi_vlog(priority, format, ap);
  va_end(ap);
}

void piksi_vlog(int priority, const char *format, va_list ap)
{
  if (log_stdout_only) {
    char *with_return = (char *)alloca(strlen(format) + 1 /* newline */ + 1 /* null terminator */);
    if (with_return != NULL) {
      sprintf(with_return, "%s\n", format);
      vprintf(with_return, ap);
    }
    return;
  }
  if ((priority & LOG_FACMASK) == LOG_SBP) {
    priority &= ~LOG_FACMASK;
    sbp_vlog(priority, format, ap);
  }

  vsyslog(priority, format, ap);
}

void sbp_log(int priority, const char *msg_text, ...)
{
  va_list ap;
  va_start(ap, msg_text);
  sbp_vlog(level, msg_text, ap);
  va_end(ap);
}

void sbp_vlog(int priority, const char *msg, ...)
{
  msg_log_t *log;
  va_list ap;
  char buf[SBP_FRAMING_MAX_PAYLOAD_SIZE];

  if (!sbp_tx) {
    piksi_log(LOG_ERR, "attempt to send SBP prior to initialization.");
    return;
  }

  if (priority < 0 || priority > UINT8_MAX) {
    piksi_log(LOG_ERR, "invalid SBP log level.");
    return;
  }

  log = (msg_log_t *)buf;
  log->level = (uint8_t)priority;

  va_start(ap, msg);
  int n = vsnprintf(log->text, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_log_t), msg, ap);
  va_end(ap);

  if (n < 0) return;

  if (0 != sbp_tx_send(sbp_tx, SBP_MSG_LOG, n + sizeof(msg_log_t), (uint8_t *)buf)) {
    piksi_log(LOG_ERR, "unable to transmit SBP message.");
  }
}
