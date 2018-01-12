/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>

#include <libsbp/sbp.h>
#include <libsbp/navigation.h>

#include "health_monitor.h"

#include "baseline_monitor.h"

#define BASELINE_THRESHOLD (80000.0f)
static health_monitor_t* baseline_monitor;

static int sbp_msg_baseline_ecef_callback(health_monitor_t* monitor, u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  (void)monitor;
  (void)sender_id;
  (void)len;
  log_fn_t log_fn = (log_fn_t)ctx;
  msg_baseline_ecef_t *msg = (void*)msg_;
  float x = msg->x, y = msg->y, z = msg->z;
  float distance = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
  if (distance > BASELINE_THRESHOLD) {
    log_fn(LOG_WARNING, "Baseline Distance Over Threshold: %fm", distance);
  }
  return 0;
}

int baseline_threshold_health_monitor_init(health_ctx_t* health_ctx)
{
  baseline_monitor = health_monitor_create();
  if (baseline_monitor == NULL) {
    return -1;
  }

  return health_monitor_init(baseline_monitor, health_ctx,
                            SBP_MSG_BASELINE_ECEF, sbp_msg_baseline_ecef_callback,
                            0, NULL, health_context_get_log(health_ctx));
}

void baseline_threshold_health_monitor_deinit(void)
{
  if (baseline_monitor != NULL) {
    health_monitor_destroy(&baseline_monitor);
  }
}


