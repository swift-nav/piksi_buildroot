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

#include <libpiksi/logging.h>

#include <libpiksi/sbp_pubsub.h>

struct sbp_pubsub_ctx_s {
  sbp_tx_ctx_t *tx_ctx;
  sbp_rx_ctx_t *rx_ctx;
};

sbp_pubsub_ctx_t *sbp_pubsub_create(const char *ident, const char *pub_ept, const char *sub_ept)
{
  return sbp_pubsub_create_ex(ident, pub_ept, sub_ept, false);
}

sbp_pubsub_ctx_t *sbp_pubsub_create_ex(const char *ident,
                                       const char *pub_ept,
                                       const char *sub_ept,
                                       bool server)
{
  assert(pub_ept != NULL);
  assert(sub_ept != NULL);

  sbp_pubsub_ctx_t *ctx = (sbp_pubsub_ctx_t *)calloc(1, sizeof(struct sbp_pubsub_ctx_s));
  if (ctx == NULL) {
    piksi_log(LOG_ERR, "error creating SBP pubsub context");
    goto failure;
  }

  ctx->tx_ctx = sbp_tx_create_ex(ident, pub_ept, server);
  if (ctx->tx_ctx == NULL) {
    piksi_log(LOG_ERR, "error creating SBP TX context");
    goto failure;
  }

  ctx->rx_ctx = sbp_rx_create_ex(ident, sub_ept, server);
  if (ctx->rx_ctx == NULL) {
    piksi_log(LOG_ERR, "error creating SBP RX context");
    goto failure;
  }

  return ctx;

failure:
  sbp_pubsub_destroy(&ctx);
  return NULL;
}

void sbp_pubsub_destroy(sbp_pubsub_ctx_t **ctx_loc)
{
  if (ctx_loc == NULL || *ctx_loc == NULL) {
    return;
  }
  sbp_pubsub_ctx_t *ctx = (sbp_pubsub_ctx_t *)(*ctx_loc);
  if (ctx->tx_ctx != NULL) sbp_tx_destroy(&ctx->tx_ctx);
  if (ctx->rx_ctx != NULL) sbp_rx_destroy(&ctx->rx_ctx);
  free(ctx);
  *ctx_loc = NULL;
}

sbp_tx_ctx_t *sbp_pubsub_tx_ctx_get(sbp_pubsub_ctx_t *ctx)
{
  assert(ctx != NULL);

  return ctx->tx_ctx;
}

sbp_rx_ctx_t *sbp_pubsub_rx_ctx_get(sbp_pubsub_ctx_t *ctx)
{
  assert(ctx != NULL);

  return ctx->rx_ctx;
}

int sbp_pubsub_attach(sbp_pubsub_ctx_t *ctx, pk_loop_t *loop)
{
  int rc = 0;
  rc = sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx), loop);
  if (rc != 0) return rc;
  rc = sbp_tx_attach(sbp_pubsub_tx_ctx_get(ctx), loop);
  return rc;
}

int sbp_pubsub_detach(sbp_pubsub_ctx_t *ctx)
{
  int rc = 0;
  rc = sbp_rx_detach(sbp_pubsub_rx_ctx_get(ctx));
  if (rc != 0) return rc;
  rc = sbp_tx_detach(sbp_pubsub_tx_ctx_get(ctx));
  return rc;
}
