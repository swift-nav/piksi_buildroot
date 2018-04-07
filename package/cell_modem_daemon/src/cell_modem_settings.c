/*
 * Copyright (C) 2018 Swift Navigation Inc.
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
#include <libpiksi/runit.h>
#include <libpiksi/util.h>
#include <libsbp/logging.h>

#include "cell_modem_settings.h"
#include "cell_modem_inotify.h"

static enum modem_type modem_type = MODEM_TYPE_GSM;
static char *cell_modem_dev;
/* External settings */
static char cell_modem_apn[32] = "hologram";
static bool cell_modem_enabled_;
static bool cell_modem_debug;

static int cell_modem_notify(void *context);

#define RUNIT_SERVICE_DIR "/var/run/cell_modem_daemon/sv"
#define RUNIT_SERVICE_NAME "pppd_daemon"

#define SBP_PAYLOAD_SIZE_MAX (255u)

enum { STORAGE_SIZE = SBP_PAYLOAD_SIZE_MAX-1 };

void cell_modem_set_dev(sbp_zmq_pubsub_ctx_t *pubsub_ctx, char *dev, enum modem_type type)
{
  cell_modem_dev = dev;
  modem_type = type;

  runit_config_t cfg_stat = (runit_config_t){
    .service_dir  = RUNIT_SERVICE_DIR,
    .service_name = RUNIT_SERVICE_NAME,
  };

  runit_stat_t stat = stat_runit_service(&cfg_stat);
  piksi_log(LOG_DEBUG, "%s: runit service status: %s", __FUNCTION__, runit_status_str(stat));

  if(cell_modem_enabled_ &&
     (cell_modem_dev != NULL) &&
     (modem_type != MODEM_TYPE_INVALID) &&
     (stat != RUNIT_RUNNING))
    cell_modem_notify(pubsub_ctx);
}

int pppd_respawn(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;

  cell_modem_notify(arg);

  return 0;
}

bool cell_modem_enabled(void)
{
  return cell_modem_enabled_;
}

static int cell_modem_notify(void *context)
{
  (void)context;

  runit_config_t cfg_stop = (runit_config_t){
    .service_dir  = RUNIT_SERVICE_DIR,
    .service_name = RUNIT_SERVICE_NAME,
  };

  stop_runit_service(&cfg_stop);

  if ((!cell_modem_enabled_) ||
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

  const char* debug_log = cell_modem_debug ? "| sbp_log --debug" : "";

  char command_line[1024];
  int count = snprintf(command_line, sizeof(command_line),
                       "sudo /usr/sbin/pppd %s connect '%s' %s", cell_modem_dev, chatcmd, debug_log);

  assert( (size_t)count < sizeof(command_line) );

  runit_config_t cfg_start = (runit_config_t){
    .service_dir  = RUNIT_SERVICE_DIR,
    .service_name = RUNIT_SERVICE_NAME,
    .command_line = command_line,
    .custom_down  = "sudo /etc/init.d/kill_pppd_service",
    .restart      = true,
  };

  return start_runit_service(&cfg_start);
}

int cell_modem_init(sbp_zmq_pubsub_ctx_t *pubsub_ctx, settings_ctx_t *settings_ctx)
{
  settings_register(settings_ctx, "cell_modem", "APN", &cell_modem_apn,
                    sizeof(cell_modem_apn), SETTINGS_TYPE_STRING,
                    cell_modem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "enable", &cell_modem_enabled_,
                    sizeof(cell_modem_enabled_), SETTINGS_TYPE_BOOL,
                    cell_modem_notify, pubsub_ctx);
  settings_register(settings_ctx, "cell_modem", "debug", &cell_modem_debug,
                    sizeof(cell_modem_debug), SETTINGS_TYPE_BOOL,
                    NULL, NULL);
  async_wait_for_tty(pubsub_ctx);
  return 0;
}
