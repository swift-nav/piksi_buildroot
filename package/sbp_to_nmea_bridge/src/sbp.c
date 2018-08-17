/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libsbp/piksi.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/loop.h>
#include <libpiksi/logging.h>

#include "sbp.h"

#define SBP_SUB_ENDPOINT    "ipc:///var/run/sockets/external.pub"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    "ipc:///var/run/sockets/external.sub"  /* SBP External In */

static struct {
  pk_loop_t *loop;
  sbp_pubsub_ctx_t *pubsub_ctx;
  settings_ctx_t *settings_ctx;
  bool simulator_enabled;
} sbp_ctx;

static struct sbp_ctx ctx_rover = {
  .loop = NULL,
  .pubsub_ctx = NULL,
  .settings_ctx = NULL,
  .simulator_enabled = false
};

static struct sbp_ctx ctx_reference = {
  .loop = NULL,
  .pubsub_ctx = NULL,
  .settings_ctx = NULL,
  .simulator_enabled = false
};

int sbp_init(void)
{
  sbp_rover_init();
  sbp_reference_init();
}

int sbp_rover_init() {
  ctx_rover.loop = pk_loop_create();
  if (ctx_rover.loop == NULL) {
    goto failure;
  }

  ctx_rover.pubsub_ctx = sbp_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx_rover.pubsub_ctx == NULL) {
    goto failure;
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx_rover.pubsub_ctx), ctx_rover.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for sbp pubsub!");
    goto failure;
  }

  ctx_rover.settings_ctx = settings_create();
  if (ctx_rover.settings_ctx == NULL) {
    piksi_log(LOG_ERR, "Error registering for settings!");
    goto failure;
  }

  if (settings_attach(ctx_rover.settings_ctx, ctx_rover.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for settings read!");
    goto failure;
  }

  return 0;

failure:
  sbp_deinit_rover();
  sbp_deinit_reference();
  return -1;
}

int sbp_reference_init() {
  ctx_reference.loop = pk_loop_create();
  if (ctx_reference.loop == NULL) {
    goto failure;
  }

  ctx_reference.pubsub_ctx = sbp_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx_reference.pubsub_ctx == NULL) {
    goto failure;
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx_reference.pubsub_ctx), ctx_reference.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for sbp pubsub!");
    goto failure;
  }

  ctx_reference.settings_ctx = settings_create();
  if (ctx_reference.settings_ctx == NULL) {
    piksi_log(LOG_ERR, "Error registering for settings!");
    goto failure;
  }

  if (settings_attach(ctx_reference.settings_ctx, ctx_reference.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for settings read!");
    goto failure;
  }

  return 0;

failure:
  sbp_deinit_rover();
  sbp_deinit_reference();
  return -1;
}


void sbp_deinit_rover(void)
{
  if (ctx_rover.loop != NULL) { pk_loop_destroy(&ctx_rover.loop); }
  if (ctx_rover.pubsub_ctx != NULL) { sbp_pubsub_destroy(&ctx_rover.pubsub_ctx); }
  if (ctx_rover.settings_ctx != NULL) { settings_destroy(&ctx_rover.settings_ctx); }
}

void sbp_deinit_reference(void)
{
  if (ctx_reference.loop != NULL) { pk_loop_destroy(&ctx_reference.loop); }
  if (ctx_reference.pubsub_ctx != NULL) { sbp_pubsub_destroy(&ctx_reference.pubsub_ctx); }
  if (ctx_reference.settings_ctx != NULL) { settings_destroy(&ctx_reference.settings_ctx); }
}

pk_loop_t * sbp_get_loop_rover(void)
{
  return ctx_rover.loop;
}

pk_loop_t * sbp_get_loop_reference(void)
{
  return ctx_reference.loop;
}

settings_ctx_t * sbp_get_settings_rover_ctx(void)
{
  return ctx_rover.settings_ctx;
}

void sbp_message_send(u16 msg_type, u8 len, u8 *payload, u16 sender_id, void *context)
{
  (void)context;
  sbp_tx_ctx_t *tx_ctx = sbp_pubsub_tx_ctx_get(ctx_rover.pubsub_ctx);
  if (tx_ctx == NULL) {
    return;
  }

  sbp_tx_send_from(tx_ctx, msg_type, len, payload, sender_id);
}

int sbp_callback_register_rover(u16 msg_type, sbp_msg_callback_t cb, void *context)
{
  sbp_rx_ctx_t *rx_ctx = sbp_pubsub_rx_ctx_get(ctx_rover.pubsub_ctx);
  if (rx_ctx == NULL) {
    return -1;
  }

  return sbp_rx_callback_register(rx_ctx, msg_type, cb, context, NULL);
}

int sbp_callback_register_reference(u16 msg_type, sbp_msg_callback_t cb, void *context)
{
  sbp_rx_ctx_t *rx_ctx = sbp_pubsub_rx_ctx_get(ctx_reference.pubsub_ctx);
  if (rx_ctx == NULL) {
    return -1;
  }

  return sbp_rx_callback_register(rx_ctx, msg_type, cb, context, NULL);
}

int sbp_run_rover(void)
{
  return pk_loop_run_simple(ctx_rover.loop);
}

int sbp_run_reference(void)
{
  return pk_loop_run_simple(ctx_reference.loop);
}
