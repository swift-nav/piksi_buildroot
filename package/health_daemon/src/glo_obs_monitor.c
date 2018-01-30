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

/**
 * \file glo_obs_monitor.c
 * \brief GLONASS Observations Health Monitor
 *
 * When glonass acquisition is enabled, it is expected that glonass obs
 * measurements will be received periodically. This monitor will track
 * base obs messages and inspect them for glonass observations, and
 * subsequently alert after a specified time period if none are
 * received. Will not alert when not connected to a base station (no base
 * obs messages being received).
 * \author Ben Altieri
 * \version v1.4.0
 * \date 2018-01-30
 */

#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>

#include <libsbp/sbp.h>
#include <libsbp/observation.h>

#include "health_monitor.h"
#include "utils.h"

#include "glo_health_context.h"
#include "glo_obs_monitor.h"

/* these are from fw private, consider moving to libpiski */
#define MSG_FORWARD_SENDER_ID (0u)

#define GLO_OBS_ALERT_RATE_LIMIT (10000u)           /* ms */
#define GLO_OBS_RESET_BASE_CONN_RATE_LIMIT (20000u) /* ms */

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

/**
 * \brief Private context for the glo obs health monitor
 */
static health_monitor_t *glo_obs_monitor;

static u8 max_timeout_count_before_reset =
  (u8)(GLO_OBS_ALERT_RATE_LIMIT / GLO_OBS_RESET_BASE_CONN_RATE_LIMIT);

/**
 * \brief Private global data for glo obs callbacks
 */
static struct glo_obs_ctx_s {
  bool glo_setting_read_resp;
  u8 timeout_counter;
} glo_obs_ctx = { .glo_setting_read_resp = false, .timeout_counter = 0 };

/**
 * \brief sbp_msg_read_resp_callback - catch glo setting read response message
 *
 * The current settings api requires us to receive any setting read response
 * and inspect it manually. This callback handles that process while also
 * checking for a specific setting - glonass_acquisition_enabled. If found,
 * this handler sets a global context to allow multiple glo monitors to share
 * the state.
 * \param sender_id: message sender id
 * \param len: length of the message in bytes
 * \param msg_[]: pointer to message data
 * \param ctx: user context associated with the monitor
 */
static void
sbp_msg_read_resp_callback(u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  (void)sender_id;
  (void)len;
  health_monitor_t *monitor = (health_monitor_t *)ctx;

  const char *section, *name, *value;
  if (health_util_parse_setting_read_resp(msg_, len, &section, &name, &value)
      == 0) {
    bool glonass_enabled;
    if (health_util_check_glonass_enabled(
          section, name, value, &glonass_enabled)
        == 0) {
      if (glonass_enabled && !glo_context_is_glonass_enabled()) {
        health_monitor_reset_timer(monitor);
      }
      glo_context_set_glonass_enabled(glonass_enabled);
      glo_obs_ctx.glo_setting_read_resp = true;
    }
  }
}

/**
 * \brief check_obs_msg_for_glo_obs - iterates an obs msg and checks for glo obs
 * \param msg: pointer reference to message buffer
 * \param len: length of the message in bytes
 * \return true if glo obs found in the message, otherwise false
 */
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

/**
 * \brief sbp_msg_glo_obs_callback - handler for glo obs sbp messages
 * \param monitor: health monitor associated with this callback
 * \param sender_id: message sender id
 * \param len: length of the message in bytes
 * \param msg_[]: pointer to message data
 * \param ctx: user context associated with the monitor
 * \return 0 resets timeout, 1 skips the reset, -1 for error
 */
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
    glo_context_receive_base_obs();
    glo_obs_ctx.timeout_counter = 0;
    if (check_obs_msg_for_glo_obs(msg_, len)) {
      return 0;
    }
  }
  return 1; // only reset if glo obs found
}

/**
 * \brief glo_obs_timer_callback - handler for glo_obs_monitor timeouts
 * \param monitor: health monitor associated with this callback
 * \param context: user context associated with this callback
 * \return 0 for success, otherwise error
 */
static int glo_obs_timer_callback(health_monitor_t *monitor, void *context)
{
  (void)context;
  if (glo_context_is_glonass_enabled() && glo_context_is_connected_to_base()) {
    sbp_log(
      LOG_WARNING,
      "Reference GLONASS Observations Timeout - no glonass observations received from base station within %d sec window",
      GLO_OBS_ALERT_RATE_LIMIT / 1000);
  }
  if (glo_context_is_connected_to_base()
      && glo_obs_ctx.timeout_counter > max_timeout_count_before_reset) {
    glo_context_reset_connected_to_base();
  }
  if (!glo_obs_ctx.glo_setting_read_resp) {
    piksi_log(LOG_DEBUG,
              "GLONASS Status Unknown - Sending GLONASS Setting Request");
    health_monitor_send_setting_read_request(
      monitor,
      SETTING_SECTION_ACQUISITION,
      SETTING_GLONASS_ACQUISITION_ENABLED);
  }
  glo_obs_ctx.timeout_counter++;

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
                          GLO_OBS_ALERT_RATE_LIMIT,
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
