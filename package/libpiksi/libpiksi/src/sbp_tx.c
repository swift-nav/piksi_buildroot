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

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

#include <libpiksi/sbp_tx.h>

#define SBP_FRAME_SIZE_MAX 264

struct sbp_tx_ctx_s {
  pk_endpoint_t *pk_ept;
  u16 sender_id;
  sbp_state_t sbp_state;
  u8 send_buffer[SBP_FRAME_SIZE_MAX];
  u32 send_buffer_length;
};

static void send_buffer_reset(sbp_tx_ctx_t *ctx)
{
  ctx->send_buffer_length = 0;
}

static u32 send_buffer_write(u8 *buff, u32 n, void *context)
{
  sbp_tx_ctx_t *ctx = (sbp_tx_ctx_t *)context;
  u32 len = SWFT_MIN(sizeof(ctx->send_buffer) - ctx->send_buffer_length, n);
  memcpy(&ctx->send_buffer[ctx->send_buffer_length], buff, len);
  ctx->send_buffer_length += len;
  return len;
}

static int send_buffer_flush(sbp_tx_ctx_t *ctx)
{
  int result = pk_endpoint_send(ctx->pk_ept, ctx->send_buffer, ctx->send_buffer_length);
  if (result == 0) {
    ctx->send_buffer_length = 0;
  }
  return result;
}

sbp_tx_ctx_t * sbp_tx_create(const char *endpoint)
{
  assert(endpoint != NULL);

  sbp_tx_ctx_t *ctx = (sbp_tx_ctx_t *)malloc(sizeof(sbp_tx_ctx_t));
  if (ctx == NULL) {
    piksi_log(LOG_ERR, "error allocating context");
    goto failure;
  }

  ctx->pk_ept = pk_endpoint_create(endpoint, PK_ENDPOINT_PUB);
  if (ctx->pk_ept == NULL) {
    piksi_log(LOG_ERR, "error creating PUB endpoint for tx ctx");
    goto failure;
  }

  ctx->sender_id = sbp_sender_id_get();

  sbp_state_init(&ctx->sbp_state);
  sbp_state_set_io_context(&ctx->sbp_state, ctx);

  return ctx;

failure:
  sbp_tx_destroy(&ctx);
  return NULL;
}

void sbp_tx_destroy(sbp_tx_ctx_t **ctx_loc)
{
  if (ctx_loc == NULL || *ctx_loc == NULL) {
    return;
  }
  sbp_tx_ctx_t *ctx = *ctx_loc;
  pk_endpoint_destroy(&ctx->pk_ept);
  free(ctx);
  *ctx_loc = NULL;
}

int sbp_tx_send(sbp_tx_ctx_t *ctx, u16 msg_type, u8 len, u8 *payload)
{
  assert(ctx != NULL);

  return sbp_tx_send_from(ctx, msg_type, len, payload, ctx->sender_id);
}

int sbp_tx_send_from(sbp_tx_ctx_t *ctx, u16 msg_type, u8 len,
                         u8 *payload, u16 sbp_sender_id)
{
  assert(ctx != NULL);

  send_buffer_reset(ctx);
  if (sbp_send_message(&ctx->sbp_state, msg_type, sbp_sender_id, len, payload,
                       send_buffer_write) != SBP_OK) {
    piksi_log(LOG_ERR, "error sending SBP message");
    return -1;
  }

  return send_buffer_flush(ctx);
}
