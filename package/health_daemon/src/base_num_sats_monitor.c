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
 * \file base_num_sats_monitor.c
 * \brief Base Observations Number of Satellites Health Monitor
 *
 * Monitors the unique satellites that are being supplied by the base station,
 * and alerts when that number is below a viable solution threshold.
 * Will not alert when not connected to a base station (no base
 * obs messages being received).
 * \author Ben Altieri
 * \version v2.2.0
 * \date 2018-09-20
 */

#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>

#include <libsbp/sbp.h>
#include <libsbp/observation.h>

#include <swiftnav/signal.h>
#include <swiftnav/sid_set.h>
#include <swiftnav/single_epoch_solver.h>

#include "health_monitor.h"

#include "glo_health_context.h"
#include "base_num_sats_monitor.h"

/* these are from fw private, consider moving to libpiski */
#define MSG_FORWARD_SENDER_ID (0u)

#define BASE_NUM_SATS_SAMPLE_RATE (10000u)      /* ms */
#define BASE_NUM_SATS_ALERT_RATE_LIMIT (30000u) /* ms */

/**
 * \brief Private context for the glo obs health monitor
 */
static health_monitor_t *base_num_sats_monitor;

/******* Local Context Helpers Begin ********/

static u8 max_timeout_count_before_warn =
  (u8)(BASE_NUM_SATS_ALERT_RATE_LIMIT / BASE_NUM_SATS_SAMPLE_RATE);

/**
 * \brief Private global data for base num sats callbacks
 */
static struct base_num_sats_ctx_s {
  gnss_sid_set_t sid_set;
  u8 timeout_counter;
} base_num_sats_ctx = {0};

static bool base_num_sats_below_threshold(void)
{
  return (sid_set_get_sat_count(&base_num_sats_ctx.sid_set) < MIN_SATS_FOR_PVT);
}

static void base_num_sats_reset(void)
{
  sid_set_init(&base_num_sats_ctx.sid_set);
}

static void base_num_sats_ingest_sid(const gnss_signal_t sid)
{
  sid_set_add(&base_num_sats_ctx.sid_set, sid);
}

static void base_num_sats_timeout_counter_increment(void)
{
  base_num_sats_ctx.timeout_counter++;
}

static bool base_num_sats_timeout_counter_above_threshold(void)
{
  return (base_num_sats_ctx.timeout_counter > max_timeout_count_before_warn);
}

static void base_num_sats_timeout_counter_reset(void)
{
  base_num_sats_ctx.timeout_counter = 0;
}

/******* Local Context Helpers Ends  ********/

/**
 * \brief check_obs_msg_for_sats - iterates an obs msg and checks for unique sat ids
 * \param msg: pointer reference to message buffer
 * \param len: length of the message in bytes
 * \return none
 */
static void check_obs_msg_for_sats(u8 *msg, u8 len)
{
  u8 obs_in_msg = (u8)(len - sizeof(observation_header_t)) / sizeof(packed_obs_content_t);
  packed_obs_content_t *obs = (packed_obs_content_t *)(msg + sizeof(observation_header_t));

  for (u8 i = 0; i < obs_in_msg; i++) {
    sbp_gnss_signal_t sbp_sid = obs[i].sid;
    gnss_signal_t sid = {
      .sat = sbp_sid.sat,
      .code = sbp_sid.code,
    };
    base_num_sats_ingest_sid(sid);
  }
}

/**
 * \brief sbp_msg_base_num_sats_callback - handler for base obs sbp messages
 * \param monitor: health monitor associated with this callback
 * \param sender_id: message sender id
 * \param len: length of the message in bytes
 * \param msg_[]: pointer to message data
 * \param ctx: user context associated with the monitor
 * \return 0 resets timeout, 1 skips the reset, -1 for error
 */
static int sbp_msg_obs_callback(health_monitor_t *monitor,
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
    check_obs_msg_for_sats(msg_, len);
  }
  return 1; /* don't reset timer as the interval is used to 'sample' num sats */
}

/**
 * \brief base_num_sats_timer_callback - handler for base_num_sats_monitor timeouts
 * \param monitor: health monitor associated with this callback
 * \param context: user context associated with this callback
 * \return 0 for success, otherwise error
 */
static int base_num_sats_timer_callback(health_monitor_t *monitor, void *context)
{
  (void)monitor;
  (void)context;

  if (glo_context_is_connected_to_base() && base_num_sats_below_threshold()) {
    if (base_num_sats_timeout_counter_above_threshold()) {
      sbp_log(
        LOG_WARNING,
        "Reference Number of Satellites low - less that %d unique satellites received from base station within %d sec window.",
        MIN_SATS_FOR_PVT,
        BASE_NUM_SATS_ALERT_RATE_LIMIT / 1000);
    } else {
      base_num_sats_timeout_counter_increment();
      return 0;
    }
  }

  base_num_sats_timeout_counter_reset();
  base_num_sats_reset();
  return 0;
}

int base_num_sats_health_monitor_init(health_ctx_t *health_ctx)
{
  base_num_sats_monitor = health_monitor_create();
  if (base_num_sats_monitor == NULL) {
    return -1;
  }

  if (health_monitor_init(base_num_sats_monitor,
                          health_ctx,
                          SBP_MSG_OBS,
                          sbp_msg_obs_callback,
                          BASE_NUM_SATS_SAMPLE_RATE,
                          base_num_sats_timer_callback,
                          NULL)
      != 0) {
    return -1;
  }

  return 0;
}

void base_num_sats_health_monitor_deinit(void)
{
  if (base_num_sats_monitor != NULL) {
    health_monitor_destroy(&base_num_sats_monitor);
  }
}
