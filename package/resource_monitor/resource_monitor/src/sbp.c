/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <uv.h>

#include <libpiksi/logging.h>
#include <libpiksi/loop.h>
#include <libpiksi/sbp_pubsub.h>

#include "sbp.h"

#define SBP_SUB_ENDPOINT "ipc:///var/run/sockets/resource_monitor.pub"
#define SBP_PUB_ENDPOINT "ipc:///var/run/sockets/resource_monitor.sub"

static uv_timer_t *uv_timer = NULL;

static struct {
  pk_loop_t *loop;
  sbp_pubsub_ctx_t *pubsub_ctx;
  settings_ctx_t *settings_ctx;
} ctx = {
  .loop = NULL,
  .pubsub_ctx = NULL,
  .settings_ctx = NULL,
};

static void signal_cb(pk_loop_t *pk_loop, void *handle, int status, void *context)
{
  (void)handle;
  (void)status;
  (void)context;

  piksi_log(LOG_DEBUG, "Received interrupt! Exiting...");
  pk_loop_stop(pk_loop);
}

bool sbp_update_timer_interval(unsigned int timer_interval, pk_loop_cb callback)
{
  if (uv_timer != NULL) pk_loop_remove_handle(uv_timer);
  uv_timer = NULL;

  if (timer_interval == 0) return true;

  uv_timer = pk_loop_timer_add(ctx.loop, timer_interval, callback, NULL);

  if (uv_timer == NULL) {
    piksi_log(LOG_ERR, "Error adding timer!");
    return false;
  }

  return true;
}

int sbp_init(unsigned int timer_interval, pk_loop_cb callback)
{
  ctx.loop = pk_loop_create();
  if (ctx.loop == NULL) {
    goto failure;
  }

  if (pk_loop_signal_handler_add(ctx.loop, SIGINT, signal_cb, NULL) == NULL) {
    piksi_log(LOG_ERR, "Error registering signal handler!");
    goto failure;
  }

  if (timer_interval != 0) {
    if (!sbp_update_timer_interval(timer_interval, callback)) {
      goto failure;
    }
  }

  ctx.pubsub_ctx = sbp_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx.pubsub_ctx == NULL) {
    goto failure;
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx.pubsub_ctx), ctx.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for sbp pubsub!");
    goto failure;
  }

  ctx.settings_ctx = settings_create();
  if (ctx.settings_ctx == NULL) {
    piksi_log(LOG_ERR, "Error registering for settings!");
    goto failure;
  }

  if (settings_attach(ctx.settings_ctx, ctx.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for settings read!");
    goto failure;
  }

  return true;

failure:
  sbp_deinit();
  return false;
}

void sbp_deinit(void)
{
  if (ctx.loop != NULL) {
    pk_loop_destroy(&ctx.loop);
  }
  if (ctx.pubsub_ctx != NULL) {
    sbp_pubsub_destroy(&ctx.pubsub_ctx);
  }
  if (ctx.settings_ctx != NULL) {
    settings_destroy(&ctx.settings_ctx);
  }
}

settings_ctx_t *sbp_get_settings_ctx(void)
{
  return ctx.settings_ctx;
}

sbp_tx_ctx_t *sbp_get_tx_ctx(void)
{
  return sbp_pubsub_tx_ctx_get(ctx.pubsub_ctx);
}

int sbp_callback_register(u16 msg_type, sbp_msg_callback_t cb, void *context)
{
  sbp_rx_ctx_t *rx_ctx = sbp_pubsub_rx_ctx_get(ctx.pubsub_ctx);
  if (rx_ctx == NULL) {
    return -1;
  }

  return sbp_rx_callback_register(rx_ctx, msg_type, cb, context, NULL);
}

int sbp_run(void)
{
  return pk_loop_run_simple(ctx.loop);
}
