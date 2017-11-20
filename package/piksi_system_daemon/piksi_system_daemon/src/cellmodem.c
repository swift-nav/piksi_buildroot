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
#include <libsbp/logging.h>

#include "cellmodem.h"
#include "cellmodem_inotify.h"
#include "async-child.h"

/* External settings */
static const char * const modem_type_names[] = {"GSM", "CDMA", NULL};
enum {MODEM_TYPE_GSM, MODEM_TYPE_CDMA};
static u8 modem_type = MODEM_TYPE_GSM;
static char cellmodem_dev[32] = "ttyACM0";
static char cellmodem_apn[32] = "INTERNET";
static bool cellmodem_enabled;
static bool cellmodem_debug;
static int cellmodem_pppd_pid;

static int cellmodem_notify(void *context);

static void pppd_output_callback(const char *buf, void *arg)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = arg;
  if (!cellmodem_debug)
    return;

  msg_log_t *msg = alloca(256);
  msg->level = 7;
  strncpy(msg->text, buf, 255);

  sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                  SBP_MSG_LOG, sizeof(*msg) + strlen(buf), (void*)msg);
}

void handle_pppd_respawn(void *arg)
{
  if(cellmodem_enabled && (cellmodem_pppd_pid == 0))
    cellmodem_notify(arg);
}

int pppd_respawn(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;

  handle_pppd_respawn(arg);

  return 0;
}

static void pppd_exit_callback(int status, void *arg)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = arg;
  cellmodem_pppd_pid = 0;
  if (!cellmodem_enabled)
    return;

  /* Respawn dead pppd */
  zloop_timer(sbp_zmq_pubsub_zloop_get(pubsub_ctx), 500, 1, pppd_respawn, pubsub_ctx);
}

static int cellmodem_notify(void *context)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = context;

  /* Kill the old pppd, if it exists. */
  if (cellmodem_pppd_pid) {
    int ret = kill(cellmodem_pppd_pid, SIGTERM);
    piksi_log(LOG_DEBUG,
              "Killing pppd with PID: %d (kill returned %d, errno %d)",
              cellmodem_pppd_pid, ret, errno);
    cellmodem_pppd_pid = 0;
  }

  if (!cellmodem_enabled)
    return 0;

  if (!cellmodem_tty_exists(cellmodem_dev)) {
    async_wait_for_tty(pubsub_ctx, cellmodem_dev);
    return 0;
  }

  char chatcmd[256];
  switch (modem_type) {
  case MODEM_TYPE_GSM:
    snprintf(chatcmd, sizeof(chatcmd),
             "/usr/sbin/chat -v -T %s -f /etc/ppp/chatscript-gsm", cellmodem_apn);
    break;
  case MODEM_TYPE_CDMA:
    strcpy(chatcmd, "/usr/sbin/chat -v -f /etc/ppp/chatscript-cdma");
    break;
  }

  /* Build pppd command line */
  char *args[] = {"/usr/sbin/pppd",
                  cellmodem_dev,
                  "connect",
                  chatcmd,
                  "usepeerdns",
                  "lcp-echo-failure", "3",
                  "lcp-echo-interval", "5",
                  NULL};

  /* Create a new pppd process. */
  async_spawn(sbp_zmq_pubsub_zloop_get(pubsub_ctx), args,
              pppd_output_callback, pppd_exit_callback, pubsub_ctx,
              &cellmodem_pppd_pid);

  return 0;
}

int cellmodem_init(sbp_zmq_pubsub_ctx_t *pubsub_ctx, settings_ctx_t *settings_ctx)
{
  settings_type_t settings_type_modem_type;
  settings_type_register_enum(settings_ctx, modem_type_names,
                              &settings_type_modem_type);
  settings_register(settings_ctx, "cell_modem", "modem_type", &modem_type,
                    sizeof(modem_type), settings_type_modem_type,
                    cellmodem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "device", &cellmodem_dev,
                    sizeof(cellmodem_dev), SETTINGS_TYPE_STRING,
                    cellmodem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "APN", &cellmodem_apn,
                    sizeof(cellmodem_apn), SETTINGS_TYPE_STRING,
                    cellmodem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "enable", &cellmodem_enabled,
                    sizeof(cellmodem_enabled), SETTINGS_TYPE_BOOL,
                    cellmodem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "debug", &cellmodem_debug,
                    sizeof(cellmodem_debug), SETTINGS_TYPE_BOOL,
                    NULL, NULL);
  return 0;
}
