/*
 * Copyright (C) 2017-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdarg.h>

#include <libpiksi/cast_check.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/sbp_tx.h>

#include <libsbp/logging.h>

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

#define SBP_TX_ENDPOINT "ipc:///var/run/sockets/internal.sub"

#define FACILITY LOG_LOCAL0
#define OPTIONS (LOG_CONS | LOG_PID | LOG_NDELAY)

#define MAX_XXD_DUMP 512

static bool log_stdout_only = false;

int logging_init(const char *identity)
{
  openlog(identity, OPTIONS, FACILITY);

  return 0;
}

void logging_log_to_stdout_only(bool enable)
{
  log_stdout_only = enable;
}

void logging_deinit(void)
{
  closelog();
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

/* Adapted from: https://www.libssh2.org/mail/libssh2-devel-archive-2011-07/att-0011/xxd.c */
void piksi_log_xxd(int priority, const char *header, const u8 *buf_in, size_t len)
{
  piksi_log(priority, "%s", header);
  char buf_start[128] = {0};
  if (len >= MAX_XXD_DUMP) {
    piksi_log(priority, "<... buffer too large for %s ...>", __FUNCTION__);
    return;
  }
  char *buf = buf_start;
  size_t i, j;
  for (i = 0; i < len; i += 16) {
    buf += sprintf(buf, "%06x: ", (unsigned int)i);
    for (j = 0; j < 16; j++)
      if (i + j < len)
        buf += sprintf(buf, " %02x", buf_in[i + j]);
      else
        buf += sprintf(buf, "   ");
    buf += sprintf(buf, "  ");
    for (j = 0; j < 16 && i + j < len; j++)
      buf += sprintf(buf, "%c", isprint(buf_in[i + j]) ? buf_in[i + j] : '.');
    piksi_log(priority, "%s", buf_start);
    buf = buf_start;
    buf[0] = '\0';
  }
}

void sbp_log(int priority, const char *msg_text, ...)
{
  va_list ap;
  va_start(ap, msg_text);
  sbp_vlog(priority, msg_text, ap);
  va_end(ap);
}

void sbp_vlog(int priority, const char *msg, va_list ap)
{
  /* TODO: Need to make metrics read pre-existing files before this will work */
  sbp_tx_ctx_t *sbp_tx = sbp_tx_create(NULL, SBP_TX_ENDPOINT);

  if (NULL == sbp_tx) {
    piksi_log(LOG_ERR, "unable to initialize SBP tx endpoint.");
    return;
  }

  /* Force main thread to sleep so libpiksi has a chance to setup... */
  usleep(1);

  msg_log_t *log;
  char buf[SBP_FRAMING_MAX_PAYLOAD_SIZE];

  if (priority < 0 || priority > UINT8_MAX) {
    piksi_log(LOG_ERR, "invalid SBP log level.");
    goto exit;
  }

  log = (msg_log_t *)buf;
  log->level = (uint8_t)priority;

  int n = vsnprintf(log->text, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_log_t), msg, ap);

  if (n < 0) goto exit;

  u8 payload_size =
    sizet_to_uint8(sizeof(msg_log_t)
                   + SWFT_MIN(int_to_sizet(n), SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_log_t)));

  if (0 != sbp_tx_send(sbp_tx, SBP_MSG_LOG, payload_size, (uint8_t *)buf)) {
    piksi_log(LOG_ERR, "unable to transmit SBP message.");
  }

exit:
  sbp_tx_destroy(&sbp_tx);
}
