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
} ctx = {
  .loop = NULL,
  .pubsub_ctx = NULL,
  .settings_ctx = NULL,
  .simulator_enabled = false,
};

int sbp_init(void)
{
  ctx.loop = pk_loop_create();
  if (ctx.loop == NULL) {
    goto failure;
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

  return 0;

failure:
  sbp_deinit();
  return -1;
}

void sbp_deinit(void)
{
  if (ctx.loop != NULL) { pk_loop_destroy(&ctx.loop); }
  if (ctx.pubsub_ctx != NULL) { sbp_pubsub_destroy(&ctx.pubsub_ctx); }
  if (ctx.settings_ctx != NULL) { settings_destroy(&ctx.settings_ctx); }
}

pk_loop_t * sbp_get_loop(void)
{
  return ctx.loop;
}

settings_ctx_t * sbp_get_settings_ctx(void)
{
  return ctx.settings_ctx;
}

void sbp_message_send(u16 msg_type, u8 len, u8 *payload, u16 sender_id, void *context)
{
  (void)context;
  sbp_tx_ctx_t *tx_ctx = sbp_pubsub_tx_ctx_get(ctx.pubsub_ctx);
  if (tx_ctx == NULL) {
    return;
  }

  sbp_tx_send_from(tx_ctx, msg_type, len, payload, sender_id);
}

int sbp_callback_register(u16 msg_type, sbp_msg_callback_t cb, void *context)
{
  sbp_rx_ctx_t *rx_ctx = sbp_pubsub_rx_ctx_get(ctx.pubsub_ctx);
  if (rx_ctx == NULL) {
    return -1;
  }

  return sbp_rx_callback_register(rx_ctx, msg_type, cb, context, NULL);
}

void sbp_simulator_enabled_set(bool enabled)
{
  ctx.simulator_enabled = enabled;
}

void sbp_base_obs_invalid(double timediff, void *context)
{
  if (ctx.simulator_enabled) {
    return;
  }

  piksi_log(LOG_WARNING, "received indication that base obs. are invalid, time difference: %f", timediff);

  static const char ntrip_sanity_failed[] = "ntrip_daemon --reconnect";
  static const size_t command_len = sizeof(ntrip_sanity_failed) - sizeof(ntrip_sanity_failed[0]);

  u8 msg_buf[sizeof(msg_command_req_t) + command_len];
  int msg_len = sizeof(msg_buf);

  msg_command_req_t* sbp_command = (msg_command_req_t*)msg_buf;
  memcpy(sbp_command->command, ntrip_sanity_failed, command_len);

  sbp_message_send(SBP_MSG_COMMAND_REQ, (u8)msg_len, (u8*)sbp_command, 0, context);
}

int sbp_run(void)
{
  return pk_loop_run_simple(ctx.loop);
}

