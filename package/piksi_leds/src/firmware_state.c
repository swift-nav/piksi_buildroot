/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/logging.h>

#include <libsbp/observation.h>
#include <libsbp/navigation.h>
#include <libsbp/system.h>
#include <libsbp/tracking.h>

#include "firmware_state.h"

/* These really belong in libsbp */
#define SBP_ECEF_FLAGS_MODE_MASK 0x7
#define SBP_HEARTBEAT_FLAGS_ANTENNA_SHIFT 31

static u8 base_obs_counter;
static struct soln_state soln_state;

static void sbp_msg_obs_callback(u16 sender_id, u8 len, u8 msg[], void *ctx)
{
  if (sender_id == 0)
    base_obs_counter++;
}

u8 firmware_state_obs_counter_get(void)
{
  return base_obs_counter;
}

static void sbp_msg_pos_ecef_callback(u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  msg_pos_ecef_t *msg = (void*)msg_;
  soln_state.spp.mode = msg->flags & SBP_ECEF_FLAGS_MODE_MASK;
  clock_gettime(CLOCK_MONOTONIC, &soln_state.spp.systime);
}

static void sbp_msg_baseline_ecef_callback(u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  msg_baseline_ecef_t *msg = (void*)msg_;
  soln_state.dgnss.mode = msg->flags & SBP_ECEF_FLAGS_MODE_MASK;
  clock_gettime(CLOCK_MONOTONIC, &soln_state.dgnss.systime);
}

static void sbp_msg_heartbeat_callback(u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  msg_heartbeat_t *msg = (void*)msg_;
  soln_state.antenna = msg->flags >> SBP_HEARTBEAT_FLAGS_ANTENNA_SHIFT;
}

static void sbp_msg_tracking_state_callback(u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  msg_tracking_state_t *msg = (void*)msg_;
  int states = len / sizeof(tracking_channel_state_t);
  int sats = 0;
  for (int i = 0; i < states; i++) {
    if ((msg->states[i].sid.sat != 0) || (msg->states[i].sid.code != 0))
      sats++;
  }
  soln_state.sats = sats;
}

void firmware_state_get(struct soln_state *out)
{
  memcpy(out, &soln_state, sizeof(*out));
}

void firmware_state_init(sbp_zmq_rx_ctx_t *ctx)
{
  sbp_zmq_rx_callback_register(ctx, SBP_MSG_OBS,
                               sbp_msg_obs_callback, NULL, NULL);
  sbp_zmq_rx_callback_register(ctx, SBP_MSG_POS_ECEF,
                               sbp_msg_pos_ecef_callback, NULL, NULL);
  sbp_zmq_rx_callback_register(ctx, SBP_MSG_BASELINE_ECEF,
                               sbp_msg_baseline_ecef_callback, NULL, NULL);
  sbp_zmq_rx_callback_register(ctx, SBP_MSG_HEARTBEAT,
                               sbp_msg_heartbeat_callback, NULL, NULL);
  sbp_zmq_rx_callback_register(ctx, SBP_MSG_TRACKING_STATE,
                               sbp_msg_tracking_state_callback, NULL, NULL);
}
