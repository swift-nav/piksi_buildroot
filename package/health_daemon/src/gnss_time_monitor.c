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
 * \file gnss_time_monitor.c
 * \brief GNSS Time Health Monitor
 *
 * This monitor will observe GNSS time related status and create and update
 * outputs for other users convenience.
 * \author Pasi Miettinen
 * \version v2.1.0
 * \date 2018-09-06
 */

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include <libsbp/sbp.h>
#include <libsbp/navigation.h>

#include "health_monitor.h"

#include "gnss_time_monitor.h"

#define GNSS_TIME_ALERT_RATE_MS 60000

#define TIME_SOURCE_MASK 0x07 /* Bits 0-2 */
#define NO_TIME 0

/**
 * \brief Private context for the gnss obs health monitor
 */
static health_monitor_t *gnss_time_monitor;

/**
 * \brief Private global data for gnss obs callbacks
 */
static struct gnss_time_ctx_s {
  u16 sbp_sender_id;
} gnss_time_ctx = {.sbp_sender_id = 0};

/**
 * \brief sbp_msg_ntrip_obs_callback - handler for obs sbp messages
 * \param monitor: health monitor associated with this callback
 * \param sender_id: message sender id
 * \param len: length of the message in bytes
 * \param msg_[]: pointer to message data
 * \param ctx: user context associated with the monitor
 * \return 0 resets timeout, 1 skips the reset, -1 for error
 */
static int sbp_msg_gps_time_cb(health_monitor_t *monitor,
                               u16 sender_id,
                               u8 len,
                               u8 msg[],
                               void *ctx)
{
  (void)monitor;
  (void)len;
  (void)ctx;

  if (gnss_time_ctx.sbp_sender_id != sender_id) {
    return 1;
  }

  const msg_gps_time_t *time = (msg_gps_time_t *)msg;
  const bool has_time = (time->flags & TIME_SOURCE_MASK) != NO_TIME;
  set_device_has_gps_time(has_time);

  return 0;
}

/**
 * \brief gps_time_timer_cb - handler for gps_time_timer_cb timeouts
 * \param monitor: health monitor associated with this callback
 * \param context: user context associated with this callback
 * \return 0 for success, otherwise error
 */
static int gps_time_timer_cb(health_monitor_t *monitor, void *context)
{
  (void)monitor;
  (void)context;

  piksi_log(LOG_DEBUG, "No MSG_GPS_TIME in %d seconds", GNSS_TIME_ALERT_RATE_MS);

  return 0;
}

int gnss_time_health_monitor_init(health_ctx_t *health_ctx)
{
  gnss_time_monitor = health_monitor_create();
  if (gnss_time_monitor == NULL) {
    return -1;
  }

  gnss_time_ctx.sbp_sender_id = sbp_sender_id_get();

  if (health_monitor_init(gnss_time_monitor,
                          health_ctx,
                          SBP_MSG_GPS_TIME,
                          sbp_msg_gps_time_cb,
                          GNSS_TIME_ALERT_RATE_MS,
                          gps_time_timer_cb,
                          NULL)
      != 0) {
    return -1;
  }

  return 0;
}

void gnss_time_health_monitor_deinit(void)
{
  if (gnss_time_monitor != NULL) {
    health_monitor_destroy(&gnss_time_monitor);
  }
}
