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

#include <libpiksi/logging.h>
#include <libpiksi/settings_client.h>
#include <libpiksi/runit.h>

#include "ota_settings.h"

#define OTA_BASE_CMD "ota_daemon --ota "
#define OTA_DBG_SWITCH "--debug "
#define OTA_URL_SWITCH "--url "

static bool ota_enabled = false;
static bool ota_debug = false;
static char ota_url[4 * 1024] = {0};

static runit_config_t runit_cfg = (runit_config_t){
  .service_dir = OTA_RUNIT_SERVICE_DIR,
  .service_name = OTA_RUNIT_SERVICE_NAME,
  .command_line = NULL,
  .custom_down = NULL,
  .restart = NULL,
};

static int ota_notify_enable(void *context)
{
  (void)context;
  int ret = 0;
  char cmd[sizeof(OTA_BASE_CMD) + sizeof(OTA_DBG_SWITCH) + sizeof(OTA_URL_SWITCH)
           + sizeof(ota_url)] = {0};

  strncat(cmd, OTA_BASE_CMD, sizeof(OTA_BASE_CMD));

  if (ota_debug) {
    strncat(cmd, OTA_DBG_SWITCH, sizeof(OTA_DBG_SWITCH));
  }

  if (strlen(ota_url) > 0) {
    strncat(cmd, OTA_URL_SWITCH, sizeof(OTA_URL_SWITCH));
    strncat(cmd, ota_url, sizeof(ota_url));
  }

  runit_cfg.command_line = cmd;

  if (ota_enabled) {
    ret = start_runit_service(&runit_cfg);
  } else if (stat_runit_service(&runit_cfg) == RUNIT_RUNNING) {
    ret = stop_runit_service(&runit_cfg);
  }

  /* Clear the pointer because it points to local char array */
  runit_cfg.command_line = NULL;

  if (ret != 0) {
    return SETTINGS_WR_SERVICE_FAILED;
  }
  return SETTINGS_WR_OK;
}

static int ota_notify_generic(void *context)
{
  (void)context;

  if (stat_runit_service(&runit_cfg) == RUNIT_RUNNING) {
    sbp_log(LOG_WARNING, "OTA must be disabled to modify settings");
    return SETTINGS_WR_MODIFY_DISABLED;
  }

  return SETTINGS_WR_OK;
}

void ota_settings(pk_settings_ctx_t *ctx)
{
  pk_settings_register(ctx,
                       "system",
                       "ota_enabled",
                       &ota_enabled,
                       sizeof(ota_enabled),
                       SETTINGS_TYPE_BOOL,
                       ota_notify_enable,
                       NULL);

  pk_settings_register(ctx,
                       "system",
                       "ota_debug",
                       &ota_debug,
                       sizeof(ota_debug),
                       SETTINGS_TYPE_BOOL,
                       ota_notify_generic,
                       NULL);

  pk_settings_register(ctx,
                       "system",
                       "ota_url",
                       &ota_url,
                       sizeof(ota_url),
                       SETTINGS_TYPE_STRING,
                       ota_notify_generic,
                       NULL);
}
