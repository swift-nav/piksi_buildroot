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
 * \file ntrip_obs_monitor.c
 * \brief NTRIP Observations Health Monitor
 *
 * When the ntrip client is enabled, it is expected that base obs
 * measurements will be received periodically. This monitor will
 * track base obs messages, and subsequently alert after a specified
 * time period if none are received.
 * \author Ben Altieri
 * \version v1.6.0
 * \date 2018-06-13
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include <libsbp/sbp.h>
#include <libsbp/navigation.h>
#include <libsbp/observation.h>

#include "health_monitor.h"

#include "ntrip_obs_monitor.h"

#define SETTING_SECTION_NTRIP "ntrip"
#define SETTING_NTRIP_ENABLE "enable"

#define TIME_SOURCE_MASK 0x07 /* Bits 0-2 */
#define NO_TIME          0

/* these are from fw private, consider moving to libpiksi */
#define MSG_FORWARD_SENDER_ID (0u)

#define NTRIP_OBS_ALERT_RATE_LIMIT (10000u)   /* ms */
#define NTRIP_OBS_PERIOD_BEFORE_WARN (20000u) /* ms */

/**
 * \brief Private context for the ntrip obs health monitor
 */
static health_monitor_t *ntrip_obs_monitor;

static u8 max_timeout_count_before_warn =
  (u8)(NTRIP_OBS_ALERT_RATE_LIMIT / NTRIP_OBS_PERIOD_BEFORE_WARN);

/**
 * \brief Private global data for ntrip obs callbacks
 */
static struct ntrip_obs_ctx_s {
  bool ntrip_enabled;
  u8 timeout_counter;
} ntrip_obs_ctx = { .ntrip_enabled = false, .timeout_counter = 0 };

/**
 * \brief notify_ntrip_enabled - notify from watch
 * \param context: pointer to ntrip_enabled
 * \return
 */
static int notify_ntrip_enabled(void *context)
{
  (void)context;
  piksi_log(LOG_DEBUG, "NTRIP Health Monitor Setting Callback! ntrip en: %d", ntrip_obs_ctx.ntrip_enabled);
  if (ntrip_obs_ctx.ntrip_enabled) {
    ntrip_obs_ctx.timeout_counter = 0;
  }
  return 0;
}

/**
 * \brief sbp_msg_ntrip_obs_callback - handler for obs sbp messages
 * \param monitor: health monitor associated with this callback
 * \param sender_id: message sender id
 * \param len: length of the message in bytes
 * \param msg_[]: pointer to message data
 * \param ctx: user context associated with the monitor
 * \return 0 resets timeout, 1 skips the reset, -1 for error
 */
static int sbp_msg_ntrip_obs_callback(health_monitor_t *monitor,
                                      u16 sender_id,
                                      u8 len,
                                      u8 msg_[],
                                      void *ctx)
{
  (void)monitor;
  (void)len;
  (void)msg_;
  (void)ctx;

  /* reset timer on base obs received from firmware */
  if (sender_id == MSG_FORWARD_SENDER_ID) {
    ntrip_obs_ctx.timeout_counter = 0;
    return 0;
  }
  return 1; /* only reset if base obs found */
}

/* This callback can be moved to other daemon if necessary */
static void sbp_gps_time_cb(u16 sender_id, u8 len, u8 msg[], void *context) {
  (void)sender_id;
  (void)context;
  (void)len;
  static u16 sbp_sender_id = 0;

  /* Read ID once */
  if (sbp_sender_id == 0) {
    sbp_sender_id = sbp_sender_id_get();
  }

  if (sbp_sender_id != sender_id) {
    return;
  }

  const msg_gps_time_t *time = (msg_gps_time_t*)msg;
  const bool has_time = (time->flags & TIME_SOURCE_MASK) != NO_TIME;
  set_device_has_gps_time(has_time);
}

/**
 * \brief ntrip_obs_timer_callback - handler for ntrip_obs_monitor timeouts
 * \param monitor: health monitor associated with this callback
 * \param context: user context associated with this callback
 * \return 0 for success, otherwise error
 */
static int ntrip_obs_timer_callback(health_monitor_t *monitor, void *context)
{
  (void)monitor;
  (void)context;

  if (!ntrip_obs_ctx.ntrip_enabled) {
    return 0;
  }

  if (!device_has_gps_time()) {
    return 0;
  }

  if (ntrip_obs_ctx.timeout_counter > max_timeout_count_before_warn) {
    piksi_log(LOG_WARNING|LOG_SBP,
              "Reference NTRIP Observations Timeout - no observations "
              "received from base station within %d sec window. Check URL "
              "and mountpoint settings, or disable NTRIP to suppress this "
              "message.",
              NTRIP_OBS_ALERT_RATE_LIMIT / 1000);
  } else {
    ntrip_obs_ctx.timeout_counter++;
  }

  return 0;
}

int ntrip_obs_timeout_health_monitor_init(health_ctx_t *health_ctx)
{
  ntrip_obs_monitor = health_monitor_create();
  if (ntrip_obs_monitor == NULL) {
    return -1;
  }

  if (health_monitor_init(ntrip_obs_monitor,
                          health_ctx,
                          SBP_MSG_OBS,
                          sbp_msg_ntrip_obs_callback,
                          NTRIP_OBS_ALERT_RATE_LIMIT,
                          ntrip_obs_timer_callback,
                          NULL)
      != 0) {
    return -1;
  }

  if (health_monitor_add_setting_watch(ntrip_obs_monitor,
                                       SETTING_SECTION_NTRIP,
                                       SETTING_NTRIP_ENABLE,
                                       &ntrip_obs_ctx.ntrip_enabled,
                                       sizeof(ntrip_obs_ctx.ntrip_enabled),
                                       SETTINGS_TYPE_BOOL,
                                       notify_ntrip_enabled,
                                       NULL)
      != 0) {
    return -1;
  }

  if (health_monitor_register_message_handler(ntrip_obs_monitor,
                                              SBP_MSG_GPS_TIME,
                                              sbp_gps_time_cb)
      != 0) {
    return -1;
  }

  return 0;
}

void ntrip_obs_timeout_health_monitor_deinit(void)
{
  if (ntrip_obs_monitor != NULL) {
    health_monitor_destroy(&ntrip_obs_monitor);
  }
}
