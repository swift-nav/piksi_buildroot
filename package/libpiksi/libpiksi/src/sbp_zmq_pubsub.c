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

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/logging.h>
#include <assert.h>

struct sbp_zmq_pubsub_ctx_s {
  sbp_zmq_tx_ctx_t *tx_ctx;
  sbp_zmq_rx_ctx_t *rx_ctx;
  zsock_t *zsock_pub;
  zsock_t *zsock_sub;
  zloop_t *zloop;
};

static void members_destroy(sbp_zmq_pubsub_ctx_t *ctx)
{
  if (ctx->zloop != NULL) {
    zloop_destroy(&ctx->zloop);
  }

  if (ctx->rx_ctx != NULL) {
    sbp_zmq_rx_destroy(&ctx->rx_ctx);
  }

  if (ctx->tx_ctx != NULL) {
    sbp_zmq_tx_destroy(&ctx->tx_ctx);
  }

  if (ctx->zsock_sub != NULL) {
    zsock_destroy(&ctx->zsock_sub);
  }

   if (ctx->zsock_pub != NULL) {
    zsock_destroy(&ctx->zsock_pub);
  }
}

static void destroy(sbp_zmq_pubsub_ctx_t **ctx)
{
  members_destroy(*ctx);
  free(*ctx);
  *ctx = NULL;
}

sbp_zmq_pubsub_ctx_t * sbp_zmq_pubsub_create(const char *pub_ept,
                                             const char *sub_ept)
{
  assert(pub_ept != NULL);
  assert(sub_ept != NULL);

  sbp_zmq_pubsub_ctx_t *ctx = (sbp_zmq_pubsub_ctx_t *)malloc(sizeof(*ctx));
  if (ctx == NULL) {
    piksi_log(LOG_ERR, "error allocating context");
    return ctx;
  }

  ctx->zsock_pub = zsock_new_pub(pub_ept);
  if (ctx->zsock_pub == NULL) {
    piksi_log(LOG_ERR, "error creating PUB socket");
    destroy(&ctx);
    return ctx;
  }

  ctx->tx_ctx = sbp_zmq_tx_create(ctx->zsock_pub);
  if (ctx->tx_ctx == NULL) {
    piksi_log(LOG_ERR, "error creating TX context");
    destroy(&ctx);
    return ctx;
  }

  ctx->zsock_sub = zsock_new_sub(sub_ept, "");
  if (ctx->zsock_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    destroy(&ctx);
    return ctx;
  }

  ctx->rx_ctx = sbp_zmq_rx_create(ctx->zsock_sub);
  if (ctx->rx_ctx == NULL) {
    piksi_log(LOG_ERR, "error creating RX context");
    destroy(&ctx);
    return ctx;
  }

  ctx->zloop = zloop_new();
  if (ctx->zloop == NULL) {
    piksi_log(LOG_ERR, "error creating zloop");
    destroy(&ctx);
    return ctx;
  }

  if (sbp_zmq_rx_reader_add(ctx->rx_ctx, ctx->zloop) != 0) {
    piksi_log(LOG_ERR, "error adding reader");
    destroy(&ctx);
    return ctx;
  }

  return ctx;
}

void sbp_zmq_pubsub_destroy(sbp_zmq_pubsub_ctx_t **ctx)
{
  assert(ctx != NULL);
  assert(*ctx != NULL);

  destroy(ctx);
}

sbp_zmq_tx_ctx_t * sbp_zmq_pubsub_tx_ctx_get(sbp_zmq_pubsub_ctx_t *ctx)
{
  assert(ctx != NULL);
  return ctx->tx_ctx;
}

sbp_zmq_rx_ctx_t * sbp_zmq_pubsub_rx_ctx_get(sbp_zmq_pubsub_ctx_t *ctx)
{
  assert(ctx != NULL);
  return ctx->rx_ctx;
}

zsock_t * sbp_zmq_pubsub_zsock_pub_get(sbp_zmq_pubsub_ctx_t *ctx)
{
  assert(ctx != NULL);
  return ctx->zsock_pub;
}

zsock_t * sbp_zmq_pubsub_zsock_sub_get(sbp_zmq_pubsub_ctx_t *ctx)
{
  assert(ctx != NULL);
  return ctx->zsock_sub;
}

zloop_t * sbp_zmq_pubsub_zloop_get(sbp_zmq_pubsub_ctx_t *ctx)
{
  assert(ctx != NULL);
  return ctx->zloop;
}
