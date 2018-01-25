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

#include "glo_obs_monitor.h"

/* these are from fw private, consider moving to libpiski */
#define MSG_FORWARD_SENDER_ID (0u)

#define GNSS_OBS_ALERT_RATE_LIMIT (10000u) /* ms */

/* Below stolen from libswiftnav/signal.h */
#define CODE_GLO_L1OF (3u)
#define CODE_GLO_L2OF (4u)

/** Is this a GLO signal?
 *
 * \param   code  Code to check.
 * \return  True if this is a GLO signal.
 */
static inline bool is_glo(u8 code)
{
  return (CODE_GLO_L1OF == code) || (CODE_GLO_L2OF == code);
}

static health_monitor_t *glo_obs_monitor;

static struct glo_obs_ctx_s {
  bool glo_setting_read_resp;
  bool glonass_enabled;
  bool base_obs_found;
} glo_obs_ctx = { .glo_setting_read_resp = false,
                  .glonass_enabled = false,
                  .base_obs_found = false };

static void
sbp_msg_read_resp_callback(u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  (void)sender_id;
  (void)len;
  health_monitor_t *monitor = (health_monitor_t *)ctx;

  const char *section, *name, *value;
  if (health_util_parse_setting_read_resp(msg_, len, &section, &name, &value)
      == 0) {
    bool last_glonass_enabled = glo_obs_ctx.glonass_enabled;
    if (health_util_check_glonass_enabled(
          section, name, value, &glo_obs_ctx.glonass_enabled)
        == 0) {
      glo_obs_ctx.glo_setting_read_resp = true;
      if (glo_obs_ctx.glonass_enabled && !last_glonass_enabled) {
        health_monitor_reset_timer(monitor);
      }
    }
  }
}

static bool check_obs_msg_for_glo_obs(u8 *msg, u8 len)
{
  u8 obs_in_msg =
    (u8)(len - sizeof(observation_header_t)) / sizeof(packed_obs_content_t);
  packed_obs_content_t *obs =
    (packed_obs_content_t *)(msg + sizeof(observation_header_t));

  for (u8 i = 0; i < obs_in_msg; i++) {
    sbp_gnss_signal_t sid = obs[i].sid;
    if (is_glo(sid.code)) {
      return true;
    }
  }

  return false;
}

static int sbp_msg_glo_obs_callback(health_monitor_t *monitor,
                                    u16 sender_id,
                                    u8 len,
                                    u8 msg_[],
                                    void *ctx)
{
  (void)monitor;
  (void)len;
  (void)msg_;
  (void)ctx;

  if (sender_id == MSG_FORWARD_SENDER_ID) {
    glo_obs_ctx.base_obs_found = true;
    if (check_obs_msg_for_glo_obs(msg_, len)) {
      return 0;
    }
  }
  return 1; // only reset if glo obs found
}

static int glo_obs_timer_callback(health_monitor_t *monitor, void *context)
{
  (void)context;
  if (glo_obs_ctx.glonass_enabled && glo_obs_ctx.base_obs_found) {
    sbp_log(
      LOG_WARNING,
      "Reference Glonass Observations Timeout - no glonass observations received from base station within %d sec window",
      GNSS_OBS_ALERT_RATE_LIMIT / 1000);
  }
  glo_obs_ctx.base_obs_found = false;
  if (!glo_obs_ctx.glo_setting_read_resp) {
    piksi_log(LOG_DEBUG,
              "Glonass Status Unknown - Sending Glonass Setting Request");
    health_monitor_send_setting_read_request(
      monitor,
      SETTING_SECTION_ACQUISITION,
      SETTING_GLONASS_ACQUISITION_ENABLED);
  }

  return 0;
}

int glo_obs_timeout_health_monitor_init(health_ctx_t *health_ctx)
{
  glo_obs_monitor = health_monitor_create();
  if (glo_obs_monitor == NULL) {
    return -1;
  }

  if (health_monitor_init(glo_obs_monitor,
                          health_ctx,
                          SBP_MSG_OBS,
                          sbp_msg_glo_obs_callback,
                          GNSS_OBS_ALERT_RATE_LIMIT,
                          glo_obs_timer_callback,
                          NULL)
      != 0) {
    return -1;
  }

  if (health_monitor_register_setting_handler(glo_obs_monitor,
                                              sbp_msg_read_resp_callback)
      != 0) {
    return -1;
  }

  return 0;
}

void glo_obs_timeout_health_monitor_deinit(void)
{
  if (glo_obs_monitor != NULL) {
    health_monitor_destroy(&glo_obs_monitor);
  }
}
