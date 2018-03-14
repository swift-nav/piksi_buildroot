/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "obs_proto_converter.h"

struct protobuf_obs_converter_ctx_s {
  msg_obs_t obs_msg;
  swiftnav_sbp_observation_MsgObs obs_proto;
  uint8_t *buffer;
  size_t buffer_n;
  uint16_t buffer_c;
  bool has_full_epoch;
};

protobuf_obs_converter_ctx_t * protobuf_obs_create(void)
{
  protobuf_obs_converter_ctx_t *ctx = (protobuf_obs_converter_ctx_t *)malloc(sizeof(protobuf_obs_converter_ctx_t));
  memset(ctx, 0, sizeof(protobuf_obs_converter_ctx_t));
  return ctx;
}

void protobuf_obs_destroy(protobuf_obs_converter_ctx_t ** ctx_loc)
{
  protobuf_obs_converter_ctx_t *ctx = *ctx_loc;
  if (ctx_loc != NULL && ctx != NULL)
  {
    if (ctx->buffer != NULL) {
      free(ctx->buffer);
    }
    free(ctx);
    *ctx_loc = NULL;
  }
}

static u8 total_packets_from_obs_msg(msg_obs_t *obs_msg)
{
  return obs_msg->header.n_obs & 0x0F;
}

static u8 packet_counter_from_obs_msg(msg_obs_t *obs_msg)
{
  return (obs_msg->header.n_obs & 0xF0) >> 4;
}

static bool compare_obs_msg_time(msg_obs_t *obs_msg1, msg_obs_t *obs_msg2)
{
  return obs_msg1->header.t.tow == obs_msg2->header.t.tow;
}

void collect_sbp_msg_obs(protobuf_obs_converter_ctx_t *ctx, msg_obs_t *obs_msg)
{
  (void)ctx;
  if (ctx == NULL || obs_msg == NULL) {
    return;
  }

  if (total_packets_from_obs_msg(&ctx->obs_msg) == 0
      || packet_counter_from_obs_msg(&ctx->obs_msg) == 0
      || !compare_obs_msg_time(&ctx->obs_msg, obs_msg)) {
    memcpy(&ctx->obs_msg, obs_msg, sizeof(*obs_msg));
    ctx->has_full_epoch = false;
  }

  //ctx->obs_proto
}

bool protobuf_obs_has_full_epoch(protobuf_obs_converter_ctx_t *ctx)
{
  return ctx->has_full_epoch;
}

swiftnav_sbp_observation_MsgObs * protobuf_obs_protobuf(protobuf_obs_converter_ctx_t *ctx)
{
  (void)ctx;
  return NULL;
}

uint32_t protobuf_obs_protobuf_length(protobuf_obs_converter_ctx_t *ctx)
{
  (void)ctx;
  return 0;
}
