/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
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
#include <sys/inotify.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/settings.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libsbp/logging.h>

#include "cell_modem_settings.h"
#include "cell_modem_inotify.h"

static enum modem_type modem_type = MODEM_TYPE_GSM;
static char *cell_modem_dev;
/* External settings */
static char cell_modem_apn[32] = "hologram";
static bool cell_modem_enabled;
static bool cell_modem_debug;
static int cell_modem_pppd_pid;

static int cell_modem_notify(void *context);

#define SBP_PAYLOAD_SIZE_MAX (255u)
enum { STORAGE_SIZE = SBP_PAYLOAD_SIZE_MAX-1 };
#if 0 // TODO: remove me
static void pppd_output_callback(const char *buf, void *arg)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = arg;

  if (!cell_modem_debug)
    return;

  size_t len = strlen(buf);
  static char storage[SBP_PAYLOAD_SIZE_MAX] = {0};

  static char* buffer = storage;
  static size_t remaining = STORAGE_SIZE;

  size_t copy_len = SWFT_MIN(remaining, len);

  memcpy(buffer, buf, copy_len);
  buffer = &buffer[copy_len];

  remaining -= copy_len;

  // Hacky workaround for things that are sent one character at a time...
  if (len == 1 && (strcmp(storage, "NO CARRIER") != 0 &&
                   strcmp(storage, "OK") != 0 &&
                   strcmp(storage, "AT") != 0 &&
                   strcmp(storage, "AT&D2") != 0))
  {
     return;
  }

  msg_log_t *msg = alloca(sizeof(storage) + 1);
  msg->level = 7;

  strncpy(msg->text, storage, sizeof(storage));

  u8 msg_size = (u8)(sizeof(*msg) + strlen(storage));
  assert( sizeof(*msg) + strlen(storage) <= UCHAR_MAX );

  sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                  SBP_MSG_LOG, msg_size, (void*)msg);

  // Reset
  buffer = &storage[0];
  remaining = STORAGE_SIZE;
  memset(buffer, 0, remaining);
}
#endif
void cell_modem_set_dev(sbp_zmq_pubsub_ctx_t *pubsub_ctx, char *dev, enum modem_type type)
{
  cell_modem_dev = dev;
  modem_type = type;

  if(cell_modem_enabled &&
     (cell_modem_dev != NULL) &&
     (modem_type != MODEM_TYPE_INVALID) &&
     (cell_modem_pppd_pid == 0))
    cell_modem_notify(pubsub_ctx);
}

int pppd_respawn(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;

  cell_modem_notify(arg);

  return 0;
}
#if 0 // TODO: remove me
static void pppd_exit_callback(int status, void *arg)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = arg;
  cell_modem_pppd_pid = 0;
  if (!cell_modem_enabled)
    return;

  /* Respawn dead pppd */
  zloop_timer(sbp_zmq_pubsub_zloop_get(pubsub_ctx), 500, 1, pppd_respawn, pubsub_ctx);
}
#endif
static int cell_modem_notify(void *context)
{
  (void)context;

  /* Kill the old pppd, if it exists. */
  if (cell_modem_pppd_pid) {
    int ret = kill(cell_modem_pppd_pid, SIGTERM);
    piksi_log(LOG_DEBUG,
              "Killing pppd with PID: %d (kill returned %d, errno %d)",
              cell_modem_pppd_pid, ret, errno);
    cell_modem_pppd_pid = 0;
  }

  if ((!cell_modem_enabled) ||
      (cell_modem_dev == NULL) ||
      (modem_type == MODEM_TYPE_INVALID)) {
    return 0;
  }

  char chatcmd[256];
  switch (modem_type) {
  case MODEM_TYPE_GSM:
    snprintf(chatcmd, sizeof(chatcmd),
             "/usr/sbin/chat -v -T %s -f /etc/ppp/chatscript-gsm", cell_modem_apn);
    break;
  case MODEM_TYPE_CDMA:
    strcpy(chatcmd, "/usr/sbin/chat -v -f /etc/ppp/chatscript-cdma");
    break;
  case MODEM_TYPE_INVALID:
  default:
    break;
  }

  // TODO/FIXME: replace with runit service
#if 0
  /* Build pppd command line */
  char *args[] = {"/usr/sbin/pppd",
                  cell_modem_dev,
                  "connect",
                  chatcmd,
                  NULL};

  /* Create a new pppd process. */
  async_spawn(sbp_zmq_pubsub_zloop_get(pubsub_ctx), args,
              pppd_output_callback, pppd_exit_callback, pubsub_ctx,
              &cell_modem_pppd_pid);
#endif
  return 0;
}

int cell_modem_init(sbp_zmq_pubsub_ctx_t *pubsub_ctx, settings_ctx_t *settings_ctx)
{
  settings_register(settings_ctx, "cell_modem", "APN", &cell_modem_apn,
                    sizeof(cell_modem_apn), SETTINGS_TYPE_STRING,
                    cell_modem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "enable", &cell_modem_enabled,
                    sizeof(cell_modem_enabled), SETTINGS_TYPE_BOOL,
                    cell_modem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "debug", &cell_modem_debug,
                    sizeof(cell_modem_debug), SETTINGS_TYPE_BOOL,
                    NULL, NULL);
  async_wait_for_tty(pubsub_ctx);
  return 0;
}
