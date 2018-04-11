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
 * \file skylark_health_monitor.c
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

#include <libnetwork.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>

#include <libsbp/sbp.h>
#include <libsbp/navigation.h>

#include "health_monitor.h"
#include "skylark_monitor.h"

#define SKYLARK_ALERT_RATE_LIMIT (5000u) /*ms*/
#define NO_FIX (0)

static health_monitor_t* skylark_monitor;
static bool no_fix = true;

static bool skylark_enabled() {

  FILE* fp = fopen("/var/run/skylark/enabled", "r");
  char buf[1] = {0};

  (void) fread(buf, sizeof(buf), 1, fp);

  if (buf[0] != '1')
    return false;

  return true;
}

static int sbp_msg_pos_llh_callback(health_monitor_t *monitor,
                                    u16 sender_id,
                                    u8 len,
                                    u8 msg_[],
                                    void *ctx)
{
  (void)monitor;
  (void)sender_id;
  (void)len;
  (void)ctx;

  msg_pos_llh_t *pos = (msg_pos_llh_t*)msg_;
  if (pos->flags != NO_FIX) {
    no_fix = false;
    return 0;
  }

  no_fix = true;
  return 1;
}

static int skylark_timer_callback(health_monitor_t *monitor, void *context)
{
  (void)monitor;
  (void)context;

  if (!no_fix) {
    return 0;
  }

  if (!skylark_enabled()) {
    piksi_log(LOG_DEBUG, "%s: skylark is not enabled", __FUNCTION__);
    return 0;
  }

  int status = 0;
  network_status_t req_status = libnetwork_request_health(SKYLARK_CONTROL_PAIR, &status);

  if(NETWORK_STATUS_SUCCESS != req_status) {
    piksi_log(LOG_WARNING, "%s: error requesting skylark health status: %d", __FUNCTION__, status);
    return 0;
  }

  piksi_log(LOG_DEBUG, "%s: skylark health status code: %d", __FUNCTION__, status);

  if (status != 404 && status != 504)
    return 0;

  sbp_log(LOG_WARNING, "Skylark Correction Service - the service cannot function without position fix");

  return 0;
}

int skylark_monitor_init(health_ctx_t *health_ctx)
{
  skylark_monitor = health_monitor_create();
  if (skylark_monitor == NULL) {
    piksi_log(LOG_DEBUG, "%s: failed to create health monitor context", __FUNCTION__);
    return -1;
  }

  if (health_monitor_init(skylark_monitor,
                          health_ctx,
                          SBP_MSG_POS_LLH,
                          sbp_msg_pos_llh_callback,
                          SKYLARK_ALERT_RATE_LIMIT,
                          skylark_timer_callback,
                          NULL)
      != 0) {
    piksi_log(LOG_DEBUG, "%s: failed to initialize health monitor", __FUNCTION__);
    return -1;
  }

  return 0;
}

void skylark_monitor_deinit(void)
{
  if (skylark_monitor != NULL) {
    health_monitor_destroy(&skylark_monitor);
  }
}
