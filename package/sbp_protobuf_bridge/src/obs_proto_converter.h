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

#ifndef __SBP_PROTO_BRIDGE_OBS_CONVERSION
#define __SBP_PROTO_BRIDGE_OBS_CONVERSION

#include <libsbp/observation.h>
#include "observation.pb.h"

typedef struct protobuf_obs_converter_ctx_s protobuf_obs_converter_ctx_t;

protobuf_obs_converter_ctx_t * protobuf_obs_create(void);
void protobuf_obs_destroy(protobuf_obs_converter_ctx_t ** ctx_loc);

void collect_sbp_msg_obs(protobuf_obs_converter_ctx_t *ctx, msg_obs_t *obs_msg);
bool protobuf_obs_has_full_epoch(protobuf_obs_converter_ctx_t *ctx);
swiftnav_sbp_observation_MsgObs * protobuf_obs_protobuf(protobuf_obs_converter_ctx_t *ctx);
uint32_t protobuf_obs_protobuf_length(protobuf_obs_converter_ctx_t *ctx);

#endif /* __SBP_PROTO_BRIDGE_OBS_CONVERSION */
