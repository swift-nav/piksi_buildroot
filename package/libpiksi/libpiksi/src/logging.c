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

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/sbp_tx.h>

#include <libsbp/logging.h>

#define MAX_IDENTITY_SIZE (64u)

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255
#define KERNEL_MAX_LOG_SIZE SBP_FRAMING_MAX_PAYLOAD_SIZE

#define SBP_TX_ENDPOINT "ipc:///var/run/sockets/internal.sub"

#define FACILITY LOG_LOCAL0
#define OPTIONS (LOG_CONS | LOG_PID | LOG_NDELAY)

#define MAX_XXD_DUMP 512

static bool log_stdout_only = false;
static char identity_[MAX_IDENTITY_SIZE] = "";

static const char *get_priority_string(int priority)
{
  switch (priority) {
  case LOG_EMERG:
    return "emerg"; /* system is unusable */
  case LOG_ALERT:
    return "alert"; /* action must be taken immediately */
  case LOG_CRIT:
    return "crit"; /* critical conditions */
  case LOG_ERR:
    return "error"; /* error conditions */
  case LOG_WARNING:
    return "warn"; /* warning conditions */
  case LOG_NOTICE:
    return "notice"; /* normal but significant condition */
  case LOG_INFO:
    return "info"; /* informational */
  case LOG_DEBUG:
    return "debug"; /* debug-level messages */
  default:
    return ""; /* unknown level - don't map to anything */
  }
}

static void set_identity(const char *identity)
{
  strncpy(identity_, identity, sizeof(identity_));
  if (strlen(identity) >= sizeof(identity_)) {
    identity_[sizeof(identity_) - 1] = '\0';
  }
}

static const char *get_identity(void)
{
  return (const char *)identity_;
}

static bool userspace_printk(const char *msg)
{
  FILE *fp = fopen("/dev/kmsg", "w");
  if (fp == NULL) {
    return false;
  }

  bool success = (fputs(msg, fp) != EOF);

  fclose(fp);

  return success;
}

static void piksi_vklog(int priority, const char *format, va_list ap)
{
  char msg[KERNEL_MAX_LOG_SIZE];

  if (format == NULL) return;
  if (priority < 0 && priority > UINT8_MAX) return;

  const char *identity = get_identity();
  const char *priority_string = get_priority_string(priority);
  char *with_preamble = (char *)alloca(strlen(identity) + 9 /* space + bracket + 'daemon.' */
                                       + strlen(priority_string) + 3 /* bracket, colon, space */
                                       + strlen(format) + 1 /* terminator */);
  sprintf(with_preamble, "%s [daemon.%s]: %s", identity, priority_string, format);

  int count = vsnprintf(msg, sizeof(msg), with_preamble, ap);
  if ((size_t)count >= sizeof(msg)) {
    msg[sizeof(msg) - 1] = '\0';
  }

  bool result = userspace_printk(msg);

  if (result != true) {
    const char *error_note = "Unable to log to kernel: %s";
    char *with_error =
      (char *)alloca(strlen(error_note) + strlen(with_preamble) + 1 /* terminator */);
    sprintf(with_error, error_note, with_preamble);
    piksi_vlog(LOG_ERR, with_error, ap);
  }
}

void piksi_klog(int priority, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  piksi_vklog(priority, format, ap);
  va_end(ap);
}

int logging_init(const char *identity)
{
  openlog(identity, OPTIONS, FACILITY);
  set_identity(identity);

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
  piksi_vlog(priority | LOG_KMSG, format, ap);
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

  int priority_nofac = priority & ~LOG_FACMASK;
  if ((priority & LOG_FACMASK) | LOG_SBP) {
    sbp_vlog(priority_nofac, format, ap);
  }

  if ((priority & LOG_FACMASK) | LOG_KMSG) {
    piksi_vklog(priority_nofac, format, ap);
  }

  vsyslog(priority_nofac, format, ap);
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
  sbp_tx_ctx_t *sbp_tx = sbp_tx_create(SBP_TX_ENDPOINT);

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

  n = SWFT_MIN(n, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_log_t));

  if (0 != sbp_tx_send(sbp_tx, SBP_MSG_LOG, n + sizeof(msg_log_t), (uint8_t *)buf)) {
    piksi_log(LOG_ERR, "unable to transmit SBP message.");
  }

exit:
  sbp_tx_destroy(&sbp_tx);
}
