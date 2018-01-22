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

#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>

#include <libsbp/sbp.h>
#include <libsbp/observation.h>

#include "health_monitor.h"
#include "utils.h"

#include "glo_bias_monitor.h"

#define GNSS_BIAS_ALERT_RATE_LIMIT (240000) /* ms */

static health_monitor_t *glo_bias_monitor;

static struct glo_bias_ctx_s {
  bool glo_setting_read_resp;
  bool glonass_enabled;
} glo_bias_ctx = {
  .glo_setting_read_resp = false,
  .glonass_enabled = false
};

static void sbp_msg_read_resp_callback(u16 sender_id,
                                       u8 len, u8 msg_[], void *ctx)
{
  (void)sender_id;
  (void)len;
  health_monitor_t *monitor = (health_monitor_t *)ctx;

  const char *section, *name, *value;
  if(health_util_parse_setting_read_resp(msg_, len,
                                         &section, &name, &value) == 0) {
    bool last_glonass_enabled = glo_bias_ctx.glonass_enabled;
    if (health_util_check_glonass_enabled(section, name, value,
                                       &glo_bias_ctx.glonass_enabled) == 0) {
      glo_bias_ctx.glo_setting_read_resp = true;
      if (glo_bias_ctx.glonass_enabled && !last_glonass_enabled) {
        health_monitor_reset_timer(monitor);
      }
    }
  }
}

static int glo_bias_timer_callback(health_monitor_t *monitor, void *context)
{
  (void)context;
  log_fn_t log_fn = health_monitor_get_log(monitor);
  if (glo_bias_ctx.glonass_enabled) {
    log_fn(LOG_WARNING,
           "Glonass Biases Msg Timeout - no biases msg receieved within %d sec window",
           GNSS_BIAS_ALERT_RATE_LIMIT/1000);
  }
  if (!glo_bias_ctx.glo_setting_read_resp)
  {
    health_monitor_send_setting_read_request(monitor,
                                             SETTING_SECTION_ACQUISITION,
                                             SETTING_GLONASS_ACQUISITION_ENABLED);
  }

  return 0;
}

int glo_bias_timeout_health_monitor_init(health_ctx_t *health_ctx)
{
  glo_bias_monitor = health_monitor_create();
  if (glo_bias_monitor == NULL) {
    return -1;
  }

  if (health_monitor_init(glo_bias_monitor, health_ctx,
                          SBP_MSG_GLO_BIASES, NULL,
                          GNSS_BIAS_ALERT_RATE_LIMIT, glo_bias_timer_callback,
                          NULL) != 0) {
    return -1;
  }

  if (health_monitor_register_setting_handler(glo_bias_monitor,
                                              sbp_msg_read_resp_callback) != 0) {
    return -1;
  }

  return 0;
}

void glo_bias_timeout_health_monitor_deinit(void)
{
  if (glo_bias_monitor != NULL) {
    health_monitor_destroy(&glo_bias_monitor);
  }
}

