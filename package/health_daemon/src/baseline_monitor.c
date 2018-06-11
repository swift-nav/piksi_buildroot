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

#include <float.h>
#include <math.h>
#include <stdlib.h>

#include <libpiksi/logging.h>

#include <libsbp/sbp.h>
#include <libsbp/navigation.h>

#include "health_monitor.h"

#include "baseline_monitor.h"

/* From libswiftnav-private - These really belong in libsbp */
/* Define Position types - currently aligned to SBP messages */
#define NO_POSITION 0
#define SPP_POSITION 1
#define DGNSS_POSITION 2
#define FLOAT_POSITION 3
#define FIXED_POSITION 4
#define POSITION_MODE_MASK 0x07 /* Bits 0-2 */

#define BASELINE_THRESHOLD_M (80000.0f)     /* m */
#define BASELINE_ALERT_RATE_LIMIT (1000u)   /* ms */

#define MM_TO_M_FLOAT(val_in_mm) ((float)(val_in_mm) / 1000.0f)
static health_monitor_t *baseline_monitor;

static struct baseline_monitor_ctx_s {
  bool past_threshold;
  float distance_over_threshold;
} baseline_monitor_ctx = { false, 0.0f };

static int sbp_msg_baseline_ecef_callback(health_monitor_t *monitor,
                                          u16 sender_id,
                                          u8 len,
                                          u8 msg_[],
                                          void *ctx)
{
  int result = 0;
  (void)monitor;
  (void)sender_id;
  (void)len;
  msg_baseline_ecef_t *msg = (msg_baseline_ecef_t *)msg_;
  (void)ctx;

  u8 fix_mode = (msg->flags & POSITION_MODE_MASK);
  switch (fix_mode) {
  case DGNSS_POSITION:
  case FLOAT_POSITION:
  case FIXED_POSITION: {
    float x_m = MM_TO_M_FLOAT(msg->x);
    float y_m = MM_TO_M_FLOAT(msg->y);
    float z_m = MM_TO_M_FLOAT(msg->z);
    float distance = sqrtf(x_m * x_m + y_m * y_m + z_m * z_m);
    if (distance > BASELINE_THRESHOLD_M) {
      baseline_monitor_ctx.distance_over_threshold = distance;
      baseline_monitor_ctx.past_threshold = true;
      result = 1;
    }
  } break;
  case NO_POSITION:
  case SPP_POSITION: {
    baseline_monitor_ctx.past_threshold = false;
  } break;
  default: {
    result = -1;
  } break;
  }

  return result;
}

static int
baseline_threshold_rate_limiting_timer_callback(health_monitor_t *monitor,
                                                void *context)
{
  (void)monitor;
  (void)context;
  if (baseline_monitor_ctx.past_threshold) {
    sbp_log(LOG_WARNING,
            "Baseline Distance Over Threshold: %.4fm",
            baseline_monitor_ctx.distance_over_threshold);
    baseline_monitor_ctx.past_threshold = false;
  }

  return 0;
}

int baseline_threshold_health_monitor_init(health_ctx_t *health_ctx)
{
  baseline_monitor = health_monitor_create();
  if (baseline_monitor == NULL) {
    return -1;
  }

  return health_monitor_init(baseline_monitor,
                             health_ctx,
                             SBP_MSG_BASELINE_ECEF,
                             sbp_msg_baseline_ecef_callback,
                             BASELINE_ALERT_RATE_LIMIT,
                             baseline_threshold_rate_limiting_timer_callback,
                             NULL);
}

void baseline_threshold_health_monitor_deinit(void)
{
  if (baseline_monitor != NULL) {
    health_monitor_destroy(&baseline_monitor);
  }
}
