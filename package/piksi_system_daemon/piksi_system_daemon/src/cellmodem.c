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

#include "cellmodem.h"
#include "cellmodem_inotify.h"
#include "async-child.h"

#define CELLMODEM_DEV_OVERRIDE_DEFAULT "ttyACM0"

const int pppd_shutdown_deadline_usec = 500e3; // 0.5 seconds

static enum modem_type modem_type = MODEM_TYPE_GSM;
static char *cellmodem_dev;

/* External settings */
static char cellmodem_apn[32] = "hologram";
static bool cellmodem_enabled = false;
static bool cellmodem_debug = false;
static char cellmodem_dev_override[PATH_MAX] = CELLMODEM_DEV_OVERRIDE_DEFAULT;
static int cellmodem_pppd_pid = 0;

/* context for rescan on override notify */
inotify_ctx_t * inotify_ctx = NULL;

static int cellmodem_notify(void *context);

#define SBP_PAYLOAD_SIZE_MAX (255u)

// One less so the buffer is always NULL terminated
enum { MAX_STR_LENGTH = SBP_PAYLOAD_SIZE_MAX-1 };

static void pppd_output_callback(const char *buf, void *arg)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = arg;

  if (!cellmodem_debug || buf == NULL)
    return;

  msg_log_t *msg = alloca(SBP_PAYLOAD_SIZE_MAX);
  memset(msg, 0, SBP_PAYLOAD_SIZE_MAX);

  msg->level = 7;
  strncpy(msg->text, buf, MAX_STR_LENGTH);

  sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                  SBP_MSG_LOG, sizeof(*msg) + strlen(msg->text), (void*)msg);
}

void cellmodem_set_dev(sbp_zmq_pubsub_ctx_t *pubsub_ctx, char *dev, enum modem_type type)
{
  cellmodem_dev = dev;
  modem_type = type;

  if(cellmodem_enabled &&
     (cellmodem_dev != NULL) &&
     (modem_type != MODEM_TYPE_INVALID) &&
     (cellmodem_pppd_pid == 0))
    cellmodem_notify(pubsub_ctx);
}

int pppd_respawn(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;

  cellmodem_notify(arg);

  return 0;
}

char * cellmodem_get_dev_override(void)
{
  return strlen(cellmodem_dev_override) == 0 ? NULL : cellmodem_dev_override;
}

static int cellmodem_notify_dev_override(void *context)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = context;
  if (inotify_ctx == NULL) {
    return 0;
  }
  // Don't allow changes if cell modem is enabled
  if (cellmodem_enabled) {
      sbp_log(LOG_WARNING, "Modem must be disabled to modify device override");
    return 1;
  }

  // override updated
  if (cellmodem_get_dev_override() != NULL) {
    if (!cellmodem_tty_exists(cellmodem_dev_override)) {
      sbp_log(LOG_WARNING,
              "Modem device override tty does not exist: '%s'",
              cellmodem_dev_override);
    }
  }
  cellmodem_set_dev_to_invalid(inotify_ctx);
  cellmodem_notify(pubsub_ctx);
  cellmodem_scan_for_modem(inotify_ctx);
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

static bool is_running(int pid)
{
  struct stat s;
  char proc_path[64];
  int count = sprintf(proc_path, "/proc/%d", pid);
  assert( count < sizeof(proc_path) );
  return stat(proc_path, &s) == 0 && (s.st_mode & S_IFMT) == S_IFDIR;
}

static bool settings_valid(bool cellmodem_enabled,
                           char* cellmodem_dev,
                           enum modem_type modem_type)
{
  return cellmodem_enabled && cellmodem_dev != NULL && modem_type != MODEM_TYPE_INVALID;
}

static int cellmodem_notify(void *context)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = context;

  /* Kill the old pppd, if it exists. */
  if (cellmodem_pppd_pid) {

    int ret = system("/usr/bin/kill_chat_command");
    piksi_log(LOG_DEBUG, "Chat command kill script returned: %d", ret);

    ret = kill(cellmodem_pppd_pid, SIGINT);
    piksi_log(LOG_DEBUG, "Sent pppd PID %d SIGINT (returned %d, errno %d)",
              cellmodem_pppd_pid, ret, errno);

    ret = kill(cellmodem_pppd_pid, SIGTERM);
    piksi_log(LOG_DEBUG, "Sent pppd PID %d SIGTERM (returned %d, errno %d)",
              cellmodem_pppd_pid, ret, errno);

    usleep(pppd_shutdown_deadline_usec);

    if (is_running(cellmodem_pppd_pid)) {

      piksi_log(LOG_DEBUG, "pppd still running, sending SIGKILL...");
      ret = kill(cellmodem_pppd_pid, SIGKILL);

      if (ret != 0) {
        piksi_log(LOG_DEBUG, "kill(pppd, SIGKILL) failed: %d, %s",
                  errno, strerror(errno));
      }
    }

    cellmodem_pppd_pid = 0;
  }

  if (!settings_valid(cellmodem_enabled, cellmodem_dev, modem_type)) {
    return 0;
  }

  char chatcmd[256];
  switch (modem_type) {
  case MODEM_TYPE_GSM:
    snprintf(chatcmd, sizeof(chatcmd),
             "/usr/bin/chat_command gsm %s", cellmodem_apn);
    break;
  case MODEM_TYPE_CDMA:
    strcpy(chatcmd, "/usr/bin/chat_command cdma");
    break;
  }

  /* Build pppd command line */
  char *args[] = {"/usr/sbin/pppd",
                  cellmodem_dev,
                  "connect",
                  chatcmd,
                  NULL};

  sleep(30);

  /* Create a new pppd process. */
  async_spawn(sbp_zmq_pubsub_zloop_get(pubsub_ctx), args,
              pppd_output_callback, pppd_exit_callback, pubsub_ctx,
              &cellmodem_pppd_pid);

  return 0;
}

int cellmodem_init(sbp_zmq_pubsub_ctx_t *pubsub_ctx, settings_ctx_t *settings_ctx)
{
  settings_register(settings_ctx, "cell_modem", "APN", &cellmodem_apn,
                    sizeof(cellmodem_apn), SETTINGS_TYPE_STRING,
                    cellmodem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "enable", &cellmodem_enabled,
                    sizeof(cellmodem_enabled), SETTINGS_TYPE_BOOL,
                    cellmodem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "debug", &cellmodem_debug,
                    sizeof(cellmodem_debug), SETTINGS_TYPE_BOOL,
                    NULL, NULL);
  settings_register(settings_ctx, "cell_modem", "device_override", &cellmodem_dev_override,
                    sizeof(cellmodem_dev_override), SETTINGS_TYPE_STRING,
                    cellmodem_notify_dev_override, pubsub_ctx);
  inotify_ctx = async_wait_for_tty(pubsub_ctx);
  return 0;
}
