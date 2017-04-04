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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <libpiksi/settings.h>
#include <libpiksi/logging.h>

/* External settings */
static char cellmodem_dev[32] = "ttyACM0";
static char cellmodem_apn[32] = "INTERNET";
static bool cellmodem_enabled;
static int cellmodem_pppd_pid;

static int cellmodem_notify(void *context)
{
  (void)context;

  /* Kill the old pppd, if it exists. */
  if (cellmodem_pppd_pid) {
    int ret = kill(cellmodem_pppd_pid, SIGHUP);
    piksi_log(LOG_DEBUG,
              "Killing pppd with PID: %d (kill returned %d, errno %d)",
              cellmodem_pppd_pid, ret, errno);
  }

  if (!cellmodem_enabled)
    return 0;

  char chatcmd[256];
  snprintf(chatcmd, sizeof(chatcmd),
           "/usr/sbin/chat -v -T %s -f /etc/ppp/chatscript", cellmodem_apn);

  /* Build pppd command line */
  char *args[] = {"/usr/sbin/pppd",
                  cellmodem_dev,
                  "connect",
                  chatcmd,
                  NULL};

  /* Create a new pppd process. */
  if (!(cellmodem_pppd_pid = fork())) {
    execvp(args[0], args);
    piksi_log(LOG_ERR, "execvp error");
    exit(EXIT_FAILURE);
  }

  return 0;
}

int cellmodem_init(settings_ctx_t *settings_ctx)
{
  settings_register(settings_ctx, "cell_modem", "device", &cellmodem_dev,
                    sizeof(cellmodem_dev), SETTINGS_TYPE_STRING,
                    cellmodem_notify, NULL);
  settings_register(settings_ctx, "cell_modem", "APN", &cellmodem_apn,
                    sizeof(cellmodem_apn), SETTINGS_TYPE_STRING,
                    cellmodem_notify, NULL);
  settings_register(settings_ctx, "cell_modem", "enable", &cellmodem_enabled,
                    sizeof(cellmodem_enabled), SETTINGS_TYPE_BOOL,
                    cellmodem_notify, NULL);
  return 0;
}
