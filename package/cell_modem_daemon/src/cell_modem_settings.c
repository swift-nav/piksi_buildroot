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

#include <libpiksi/settings.h>
#include <libpiksi/logging.h>
#include <libpiksi/runit.h>
#include <libpiksi/util.h>
#include <libsbp/logging.h>

#include "cell_modem_settings.h"
#include "cell_modem_inotify.h"

#define CELL_MODEM_DEV_OVERRIDE_DEFAULT "ttyACM0"
#define CELL_MODEM_MAX_BOOT_RETRIES (10u)

static enum modem_type modem_type = MODEM_TYPE_GSM;
static char *cell_modem_dev;
/* External settings */
static char cell_modem_apn[32] = "hologram";
static bool cell_modem_enabled_;
static bool cell_modem_debug;
static char cell_modem_dev_override[PATH_MAX] = CELL_MODEM_DEV_OVERRIDE_DEFAULT;

/* context for rescan on override notify */
inotify_ctx_t * inotify_ctx = NULL;

static int cell_modem_notify(void *context);

#define RUNIT_SERVICE_DIR "/var/run/cell_modem_daemon/sv"
#define RUNIT_SERVICE_NAME "pppd_daemon"

#define SBP_PAYLOAD_SIZE_MAX (255u)

enum { STORAGE_SIZE = SBP_PAYLOAD_SIZE_MAX-1 };

void cell_modem_set_dev(char *dev, enum modem_type type)
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
    cell_modem_notify(NULL);
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
             "/usr/bin/chat_command gsm %s", cell_modem_apn);
    break;
  case MODEM_TYPE_CDMA:
    strcpy(chatcmd, "/usr/bin/chat_command cdma");
    break;
  case MODEM_TYPE_INVALID:
  default:
    break;
  }

  const char* debug_log = cell_modem_debug ? "| sbp_log --debug" : "";

  char command_line[1024];
  int count = snprintf(command_line, sizeof(command_line),
                       "sudo /usr/sbin/pppd %s connect '%s' %s %s",
                       cell_modem_dev, chatcmd,
                       cell_modem_debug ? "debug" : "",
                       debug_log);

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

int override_probe_retry(zloop_t *loop, int timer_id, void *arg)
{
  static int probe_retries = 0;
  inotify_ctx_t *ctx = (inotify_ctx_t*) arg;
  if (cellmodem_get_dev_override() != NULL
      && cellmodem_dev == NULL
      && probe_retries++ < CELL_MODEM_MAX_BOOT_RETRIES) {
    cellmodem_scan_for_modem(inotify_ctx);
  } else {
    piksi_log(LOG_DEBUG, "Ending override probe retry timer after %d attempts", probe_retries);
    zloop_timer_end(loop, timer_id);
  }

  return 0;
}

char * cell_modem_get_dev_override(void)
{
  return strlen(cell_modem_dev_override) == 0 ? NULL : cell_modem_dev_override;
}

static int cell_modem_notify_dev_override(void *context)
{
  inotify_ctx_t *ctx = *((inotify_ctx_t **)context);
  if (ctx == NULL) {
    return 0;
  }
  // Don't allow changes if cell modem is enabled
  if (cell_modem_enabled_) {
      sbp_log(LOG_WARNING, "Modem must be disabled to modify device override");
    return 1;
  }

  // override updated
  if (cell_modem_get_dev_override() != NULL) {
    if (!cell_modem_tty_exists(cell_modem_dev_override)) {
      sbp_log(LOG_WARNING,
              "Modem device override tty does not exist: '%s'",
              cell_modem_dev_override);
    }
  }
  cell_modem_set_dev_to_invalid(ctx);
  cell_modem_notify(NULL);
  cell_modem_scan_for_modem(ctx);
  return 0;
}

int cell_modem_init(sbp_zmq_pubsub_ctx_t *pubsub_ctx, settings_ctx_t *settings_ctx)
{
  settings_register(settings_ctx, "cell_modem", "APN", &cell_modem_apn,
                    sizeof(cell_modem_apn), SETTINGS_TYPE_STRING,
                    cell_modem_notify, NULL);
  settings_register(settings_ctx, "cell_modem", "enable", &cell_modem_enabled_,
                    sizeof(cell_modem_enabled_), SETTINGS_TYPE_BOOL,
                    cell_modem_notify, NULL);
  settings_register(settings_ctx, "cell_modem", "debug", &cell_modem_debug,
                    sizeof(cell_modem_debug), SETTINGS_TYPE_BOOL,
                    NULL, NULL);
  settings_register(settings_ctx, "cell_modem", "device_override", &cell_modem_dev_override,
                    sizeof(cell_modem_dev_override), SETTINGS_TYPE_STRING,
                    cell_modem_notify_dev_override, &inotify_ctx);
  inotify_ctx = async_wait_for_tty(pubsub_ctx);
  return 0;
}
