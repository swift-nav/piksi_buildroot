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

static struct {
  sbp_zmq_rx_ctx_t *rx_ctx;
  sbp_zmq_tx_ctx_t *tx_ctx;
} ctx = {
  .rx_ctx = NULL,
  .tx_ctx = NULL
};

static sbp_zmq_rx_ctx_t *rx_ctx = NULL;
static sbp_zmq_tx_ctx_t *tx_ctx = NULL;

int sbp_init(sbp_zmq_rx_ctx_t *rx_ctx, sbp_zmq_tx_ctx_t *tx_ctx)
{
  ctx.rx_ctx = rx_ctx;
  ctx.tx_ctx = tx_ctx;
  return 0;
}

int sbp_message_send(u16 msg_type, u8 len, u8 *payload)
{
  if (ctx.tx_ctx == NULL) {
    return -1;
  }

  return sbp_zmq_tx_send(ctx.tx_ctx, msg_type, len, payload);
}

int sbp_callback_register(u16 msg_type, sbp_msg_callback_t cb, void *context)
{
  if (ctx.rx_ctx == NULL) {
    return -1;
  }

  return sbp_zmq_rx_callback_register(ctx.rx_ctx, msg_type, cb, context, NULL);
}
