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

static sbp_zmq_tx_ctx_t *ctx = NULL;

int sbp_init(sbp_zmq_tx_ctx_t *tx_ctx)
{
  ctx = tx_ctx;
}

int sbp_message_send(u16 msg_type, u8 len, u8 *payload)
{
  if (ctx == NULL) {
    return -1;
  }

  return sbp_zmq_tx_send(ctx, msg_type, len, payload);
}
