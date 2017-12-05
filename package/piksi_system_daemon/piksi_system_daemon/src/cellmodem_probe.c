/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdarg.h>

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/logging.h>
#include <libsbp/logging.h>

#include "cellmodem.h"
#include "cellmodem_probe.h"

enum {
  RESPONSE_OK,
  RESPONSE_ERROR,
};

static const char *const response_strs[] = {
  [RESPONSE_OK] = "OK\r\n",
  [RESPONSE_ERROR] = "ERROR\r\n",
  NULL
};

static int modem_read(int fd, char *buf, size_t len, const char *const *responses)
{
  int n = 0;
  fd_set fds;
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000};
  do {
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd+1, &fds, NULL, NULL, &timeout) < 0)
      return -2;
    if (!FD_ISSET(fd, &fds)) {
      return -1;
    }
    int r = read(fd, buf + n, len - n - 1);
    if (r < 0) {
        return -3;
    }
    n += r;
    buf[n] = 0;
    for(int i = 0; responses[i]; i++) {
      int rlen = strlen(responses[i]);
      if ((n >= rlen) && (strcmp(buf + n - rlen, responses[i]) == 0)) {
        return i;
      }
    }
  } while (n < len - 1);
}

#define DELIM "\r\n"
static int cellmodem_command(int fd, const char *cmd, char *response, size_t len)
{
  /* Send command */
  write(fd, cmd, strlen(cmd));
  write(fd, "\r", 1);

  /* Read full response */
  char buf[256] = {0};
  if (modem_read(fd, buf, sizeof(buf), response_strs) != RESPONSE_OK)
    return -1;

  /* Check command echo */
  char *tmp = strtok(buf, DELIM);
  if ((tmp == NULL) || (strcmp(tmp, cmd) != 0))
    return -2;

  /* Pull out response */
  if (response) {
    tmp = strtok(NULL, DELIM);
    if (tmp == NULL)
      return -3;
    strncpy(response, tmp, len);
  }

  /* Check for OK response */
  tmp = strtok(NULL, DELIM);
  if ((tmp == NULL) || (strcmp(tmp, "OK") != 0))
    return -4;

  return 0;
}

static void log_info(sbp_zmq_pubsub_ctx_t *pubsub_ctx, const char *fmt, ...)
{
  msg_log_t *msg = alloca(256);
  msg->level = 6;

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg->text, 255, fmt, ap);
  va_end(ap);

  sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                 SBP_MSG_LOG, sizeof(*msg) + strlen(msg->text), (void*)msg);
}

enum modem_type cellmodem_probe(const char *dev, sbp_zmq_pubsub_ctx_t *pubsub_ctx)
{
  if ((strncmp(dev, "ttyUSB", 6) != 0) && (strncmp(dev, "ttyACM", 6) != 0))
    return MODEM_TYPE_INVALID;

  piksi_log(LOG_DEBUG, "%s trying /dev/%s", __func__, dev);
  char fulldev[PATH_MAX];
  snprintf(fulldev, sizeof(fulldev), "/dev/%s", dev);
  int fd = open(fulldev, O_RDWR);
  if (fd < 0) {
    piksi_log(LOG_WARNING, "%s: Failed to open %s: %s", __func__, fulldev, strerror(errno));
    return MODEM_TYPE_INVALID;
  }
  if (!isatty(fd)) {
    close(fd);
    return MODEM_TYPE_INVALID;
  }

  /* Set to raw mode */
  struct termios tio = {0};
  cfmakeraw(&tio);
  cfsetspeed(&tio, B115200);
  if (tcsetattr(fd, TCSAFLUSH, &tio) < 0) {
    piksi_log(LOG_WARNING, "%s: tcsetattr failed on %s: %s", __func__, dev, strerror(errno));
    close(fd);
    return MODEM_TYPE_INVALID;
  }

  /* Probe modem for standard parameters */
  struct command {
    char *cmd;
    char *display;
  } commands[] = {
    {"AT+CGMI", "Manufacturer"},
    {"AT+CGMM", "Model"},
    {"AT+CGMR", "Revision"},
    {"AT+CGSN", "Serial Number"},
    {NULL, NULL}
  };
  for (struct command *c = commands; c->cmd; c++) {
    char r[20];
    if (cellmodem_command(fd, c->cmd, r, sizeof(r)) < 0) {
      close(fd);
      return MODEM_TYPE_INVALID;
    }
    piksi_log(LOG_INFO, "Modem %s: %s", c->display, r);
    log_info(pubsub_ctx, "Modem %s: %s", c->display, r);
  }

  /* Check for GSM/CDMA: Try GSM 'AT+CGDCONT?', ignore the response,
   * but check for OK */
  if (cellmodem_command(fd, "AT+CGDCONT?", NULL, 0) == 0) {
    close(fd);
    return MODEM_TYPE_GSM;
  }
  close(fd);
  return MODEM_TYPE_CDMA;
}
