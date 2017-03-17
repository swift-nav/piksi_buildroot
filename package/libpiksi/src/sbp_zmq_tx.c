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

#include "sbp_zmq_tx.h"
#include "util.h"
#include <stdio.h>
#include <assert.h>

#define SBP_FRAME_SIZE_MAX 264

struct sbp_zmq_tx_ctx_s {
  zsock_t *zsock;
  u16 sender_id;
  sbp_state_t sbp_state;
  u8 send_buffer[SBP_FRAME_SIZE_MAX];
  u32 send_buffer_length;
};

static void send_buffer_reset(sbp_zmq_tx_ctx_t *ctx)
{
  ctx->send_buffer_length = 0;
}

static u32 send_buffer_write(u8 *buff, u32 n, void *context)
{
  sbp_zmq_tx_ctx_t *ctx = (sbp_zmq_tx_ctx_t *)context;
  u32 len = MIN(sizeof(ctx->send_buffer) - ctx->send_buffer_length, n);
  memcpy(&ctx->send_buffer[ctx->send_buffer_length], buff, len);
  ctx->send_buffer_length += len;
  return len;
}

static int send_buffer_flush(sbp_zmq_tx_ctx_t *ctx)
{
  zmsg_t *msg = zmsg_new();
  if (msg == NULL) {
    printf("error in zmsg_new()\n");
    return -1;
  }

  if (zmsg_addmem(msg, ctx->send_buffer, ctx->send_buffer_length) != 0) {
    printf("error in zmsg_addmem()\n");
    zmsg_destroy(&msg);
    return -1;
  }

  while (1) {
    int ret = zmsg_send(&msg, ctx->zsock);
    if (ret == 0) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      printf("error in zmsg_send()\n");
      zmsg_destroy(&msg);
      return ret;
    }
  }

  ctx->send_buffer_length = 0;
  return 0;
}

sbp_zmq_tx_ctx_t * sbp_zmq_tx_create(zsock_t *zsock)
{
  assert(zsock != NULL);
  assert(zsock_is(zsock));

  sbp_zmq_tx_ctx_t *ctx = (sbp_zmq_tx_ctx_t *)malloc(sizeof(*ctx));
  if (ctx == NULL) {
    printf("error allocating context\n");
    return ctx;
  }

  ctx->zsock = zsock;
  ctx->sender_id = sbp_sender_id_get();

  sbp_state_init(&ctx->sbp_state);
  sbp_state_set_io_context(&ctx->sbp_state, ctx);

  return ctx;
}

void sbp_zmq_tx_destroy(sbp_zmq_tx_ctx_t **ctx)
{
  assert(ctx != NULL);
  assert(*ctx != NULL);

  free(*ctx);
  *ctx = NULL;
}

int sbp_zmq_tx_send(sbp_zmq_tx_ctx_t *ctx, u16 msg_type, u8 len, u8 *payload)
{
  assert(ctx != NULL);

  return sbp_zmq_tx_send_from(ctx, msg_type, len, payload, ctx->sender_id);
}

int sbp_zmq_tx_send_from(sbp_zmq_tx_ctx_t *ctx, u16 msg_type, u8 len,
                         u8 *payload, u16 sbp_sender_id)
{
  assert(ctx != NULL);

  send_buffer_reset(ctx);
  if (sbp_send_message(&ctx->sbp_state, msg_type, sbp_sender_id, len, payload,
                       send_buffer_write) != SBP_OK) {
    printf("error sending SBP message\n");
    return -1;
  }

  return send_buffer_flush(ctx);
}
