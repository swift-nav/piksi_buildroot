/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "sbp.h"

#include <libpiksi/logging.h>
#include <libsbp/piksi.h>

static struct {
  sbp_zmq_rx_ctx_t *rx_ctx;
  sbp_zmq_tx_ctx_t *tx_ctx;
} ctx = {
  .rx_ctx = NULL,
  .tx_ctx = NULL
};

int sbp_init(sbp_zmq_rx_ctx_t *rx_ctx, sbp_zmq_tx_ctx_t *tx_ctx)
{
  if(rx_ctx == NULL || tx_ctx == NULL){
    return -1;
  }
  ctx.rx_ctx = rx_ctx;
  ctx.tx_ctx = tx_ctx;
  return 0;
}

void sbp_message_send(u8 msg_type, u8 len, u8 *payload, u16 sender_id)
{
  if (ctx.tx_ctx == NULL) {
    return;
  }

  sbp_zmq_tx_send_from(ctx.tx_ctx, msg_type, len, payload, sender_id);
}

int sbp_callback_register(u16 msg_type, sbp_msg_callback_t cb, void *context)
{
  if (ctx.rx_ctx == NULL) {
    return -1;
  }

  return sbp_zmq_rx_callback_register(ctx.rx_ctx, msg_type, cb, context, NULL);
}

void sbp_base_obs_invalid(double timediff)
{
  piksi_log(LOG_WARNING, "received indication that base obs. are invalid, time difference: %f", timediff);

  static const char ntrip_sanity_failed[] = "<<BASE_OBS_SANITY_FAILED>>";
  static const size_t command_len = sizeof(ntrip_sanity_failed) - sizeof(ntrip_sanity_failed[0]);

  u8 msg_buf[sizeof(msg_command_req_t) + command_len];
  int msg_len = sizeof(msg_buf);
  
  msg_command_req_t* sbp_command = (msg_command_req_t*)msg_buf;
  memcpy(sbp_command->command, ntrip_sanity_failed, command_len);

  sbp_message_send(SBP_MSG_COMMAND_REQ, (u8)msg_len, (u8*)sbp_command, 0);
}
